// Copyright 2014 The Chromium Authors. All rights reserved.
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

TEST(PepperPermissionUtilTest, ExtensionWhitelisting) {
  ScopedCurrentChannel current_channel(version_info::Channel::UNKNOWN);
  ExtensionSet extensions;
  std::string whitelisted_id =
      crx_file::id_util::GenerateId("whitelisted_extension");
  std::unique_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Whitelisted Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  scoped_refptr<const Extension> ext = ExtensionBuilder()
                                           .SetManifest(std::move(manifest))
                                           .SetID(whitelisted_id)
                                           .Build();
  extensions.Insert(ext);
  std::set<std::string> whitelist;
  std::string url = std::string("chrome-extension://") + whitelisted_id +
                    std::string("/manifest.nmf");
  std::string bad_scheme_url =
      std::string("http://") + whitelisted_id + std::string("/manifest.nmf");
  std::string bad_host_url = std::string("chrome-extension://") +
                             crx_file::id_util::GenerateId("bad_host");
  std::string("/manifest.nmf");

  EXPECT_FALSE(
      IsExtensionOrSharedModuleWhitelisted(GURL(url), &extensions, whitelist));
  whitelist.insert(whitelisted_id);
  EXPECT_TRUE(
      IsExtensionOrSharedModuleWhitelisted(GURL(url), &extensions, whitelist));
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(bad_scheme_url), &extensions, whitelist));
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(bad_host_url), &extensions, whitelist));
}

TEST(PepperPermissionUtilTest, SharedModuleWhitelisting) {
  ScopedCurrentChannel current_channel(version_info::Channel::UNKNOWN);
  ExtensionSet extensions;
  std::string whitelisted_id = crx_file::id_util::GenerateId("extension_id");
  std::string bad_id = crx_file::id_util::GenerateId("bad_id");

  std::unique_ptr<base::DictionaryValue> shared_module_manifest =
      DictionaryBuilder()
          .Set("name", "Whitelisted Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder()
                   .Set("resources", ListBuilder().Append("*").Build())
                   // Add the extension to the whitelist.  This
                   // restricts import to |whitelisted_id| only.
                   .Set("whitelist",
                        ListBuilder().Append(whitelisted_id).Build())
                   .Build())
          .Build();
  scoped_refptr<const Extension> shared_module =
      ExtensionBuilder().SetManifest(std::move(shared_module_manifest)).Build();

  scoped_refptr<const Extension> ext =
      CreateExtensionImportingModule(shared_module->id(), whitelisted_id);
  std::string extension_url =
      std::string("chrome-extension://") + ext->id() + std::string("/foo.html");

  std::set<std::string> whitelist;
  // Important: whitelist *only* the shared module.
  whitelist.insert(shared_module->id());

  extensions.Insert(ext);
  // This should fail because shared_module is not in the set of extensions.
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
  extensions.Insert(shared_module);
  EXPECT_TRUE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
  scoped_refptr<const Extension> not_in_sm_whitelist =
      CreateExtensionImportingModule(shared_module->id(), bad_id);
  std::string not_in_sm_whitelist_url = std::string("chrome-extension://") +
                                        not_in_sm_whitelist->id() +
                                        std::string("/foo.html");

  extensions.Insert(not_in_sm_whitelist);
  // This should succeed, even though |not_in_sm_whitelist| is not whitelisted
  // to use shared_module, because the pepper permission utility does not care
  // about that whitelist.  It is possible to install against the whitelist as
  // an unpacked extension.
  EXPECT_TRUE(IsExtensionOrSharedModuleWhitelisted(
      GURL(not_in_sm_whitelist_url), &extensions, whitelist));

  // Note that the whitelist should be empty after this call, so tests checking
  // for failure to import will fail because of this.
  whitelist.erase(shared_module->id());
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
}

}  // namespace extensions
