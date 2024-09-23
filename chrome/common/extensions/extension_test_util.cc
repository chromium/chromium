// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_test_util.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::Extension;
using extensions::Manifest;
using extensions::mojom::ManifestLocation;

namespace extension_test_util {

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               ManifestLocation location,
                                               int extra_flags,
                                               const std::string& id,
                                               std::string* error) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions")
             .AppendASCII(dir)
             .AppendASCII(test_file);

  JSONFileValueDeserializer deserializer(path);
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, error);
  if (!result)
    return nullptr;
  const base::Value::Dict* dict = result->GetIfDict();
  CHECK(dict);

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), location, *dict, extra_flags, id, error);
  return extension;
}

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               ManifestLocation location,
                                               int extra_flags,
                                               std::string* error) {
  return LoadManifestUnchecked(
      dir, test_file, location, extra_flags, std::string(), error);
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file,
                                      ManifestLocation location,
                                      int extra_flags) {
  std::string error;
  scoped_refptr<Extension> extension =
      LoadManifestUnchecked(dir, test_file, location, extra_flags, &error);

  EXPECT_TRUE(extension.get()) << test_file << ":" << error;
  return extension;
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file,
                                      int extra_flags) {
  return LoadManifest(dir, test_file, ManifestLocation::kInvalidLocation,
                      extra_flags);
}

scoped_refptr<Extension> LoadManifestStrict(const std::string& dir,
                                            const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file) {
  return LoadManifest(dir, test_file, Extension::NO_FLAGS);
}

void SetGalleryUpdateURL(const GURL& new_url) {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kAppsGalleryUpdateURL,
                                  new_url.spec());
  extensions::ExtensionsClient::Get()->InitializeWebStoreUrls(command_line);
}

// Note: This list should be kept in sync with the set of all features which
// have delegated availability checks. This includes controlled_frame and
// webstore_overide.
std::vector<const char*> GetExpectedDelegatedFeaturesForTest() {
  return {
      // Controlled frame:
      "chromeWebViewInternal",
      "controlledFrameInternal",
      "guestViewInternal",
      "webRequestInternal",
      "webViewInternal",

      // Webstore override:
      "management",
      "webstorePrivate",
  };
}

}  // namespace extension_test_util
