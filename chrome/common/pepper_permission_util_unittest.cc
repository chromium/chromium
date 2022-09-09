// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_permission_util.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "components/crx_file/id_util.h"
#include "components/version_info/version_info.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/features/feature_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// Return an extension with |id| which imports a module with the given
// |import_id|.
scoped_refptr<const Extension> CreateExtensionImportingModule(
    const std::string& import_id,
    const std::string& id) {
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Has Dependent Modules")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("import",
               ListBuilder()
                   .Append(DictionaryBuilder().Set("id", import_id).Build())
                   .Build())
          .Build();

  return ExtensionBuilder()
      .SetManifest(std::move(manifest))
      .AddFlags(Extension::FROM_WEBSTORE)
      .SetID(id)
      .Build();
}

}  // namespace

TEST(PepperPermissionUtilTest, ExtensionAllowed) {
  ScopedCurrentChannel current_channel(version_info::Channel::UNKNOWN);
  ExtensionSet extensions;
  std::string allowed_id = crx_file::id_util::GenerateId("allowed_extension");
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Allowed Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  scoped_refptr<const Extension> ext = ExtensionBuilder()
                                           .SetManifest(std::move(manifest))
                                           .SetID(allowed_id)
                                           .Build();
  extensions.Insert(ext);
  std::set<std::string> allowlist;
  std::string url = std::string("chrome-extension://") + allowed_id +
                    std::string("/manifest.nmf");
  std::string bad_scheme_url =
      std::string("http://") + allowed_id + std::string("/manifest.nmf");
  std::string bad_host_url = std::string("chrome-extension://") +
                             crx_file::id_util::GenerateId("bad_host");
  std::string("/manifest.nmf");

  EXPECT_FALSE(
      IsExtensionOrSharedModuleAllowed(GURL(url), &extensions, allowlist));
  allowlist.insert(allowed_id);
  EXPECT_TRUE(
      IsExtensionOrSharedModuleAllowed(GURL(url), &extensions, allowlist));
  EXPECT_FALSE(IsExtensionOrSharedModuleAllowed(GURL(bad_scheme_url),
                                                &extensions, allowlist));
  EXPECT_FALSE(IsExtensionOrSharedModuleAllowed(GURL(bad_host_url), &extensions,
                                                allowlist));
}

TEST(PepperPermissionUtilTest, SharedModuleAllowed) {
  ScopedCurrentChannel current_channel(version_info::Channel::UNKNOWN);
  ExtensionSet extensions;
  std::string allowed_id = crx_file::id_util::GenerateId("extension_id");
  std::string bad_id = crx_file::id_util::GenerateId("bad_id");

  std::unique_ptr<base::DictionaryValue> shared_module_manifest =
      DictionaryBuilder()
          .Set("name", "Allowed Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder()
                   .Set("resources", ListBuilder().Append("*").Build())
                   // Add the extension to the allowlist.  This
                   // restricts import to |allowed_id| only.
                   .Set("whitelist", ListBuilder().Append(allowed_id).Build())
                   .Build())
          .Build();
  scoped_refptr<const Extension> shared_module =
      ExtensionBuilder().SetManifest(std::move(shared_module_manifest)).Build();

  scoped_refptr<const Extension> ext =
      CreateExtensionImportingModule(shared_module->id(), allowed_id);
  std::string extension_url =
      std::string("chrome-extension://") + ext->id() + std::string("/foo.html");

  std::set<std::string> allowlist;
  // Important: allow *only* the shared module.
  allowlist.insert(shared_module->id());

  extensions.Insert(ext);
  // This should fail because shared_module is not in the set of extensions.
  EXPECT_FALSE(IsExtensionOrSharedModuleAllowed(GURL(extension_url),
                                                &extensions, allowlist));
  extensions.Insert(shared_module);
  EXPECT_TRUE(IsExtensionOrSharedModuleAllowed(GURL(extension_url), &extensions,
                                               allowlist));
  scoped_refptr<const Extension> not_in_sm_allowlist =
      CreateExtensionImportingModule(shared_module->id(), bad_id);
  std::string not_in_sm_allowlist_url = std::string("chrome-extension://") +
                                        not_in_sm_allowlist->id() +
                                        std::string("/foo.html");

  extensions.Insert(not_in_sm_allowlist);
  // This should succeed, even though |not_in_sm_allowlist| is not allowed
  // to use shared_module, because the pepper permission utility does not care
  // about that allowlist.  It is possible to install against the allowlist as
  // an unpacked extension.
  EXPECT_TRUE(IsExtensionOrSharedModuleAllowed(GURL(not_in_sm_allowlist_url),
                                               &extensions, allowlist));

  // Note that the allowlist should be empty after this call, so tests checking
  // for failure to import will fail because of this.
  allowlist.erase(shared_module->id());
  EXPECT_FALSE(IsExtensionOrSharedModuleAllowed(GURL(extension_url),
                                                &extensions, allowlist));
}

}  // namespace extensions
