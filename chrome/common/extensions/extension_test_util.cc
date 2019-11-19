// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "components/version_info/channel.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using extensions::Extension;
using extensions::Manifest;

namespace extension_test_util {

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               Manifest::Location location,
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
  const base::DictionaryValue* dict;
  CHECK(result->GetAsDictionary(&dict));

  scoped_refptr<Extension> extension = Extension::Create(
      path.DirName(), location, *dict, extra_flags, id, error);
  return extension;
}

scoped_refptr<Extension> LoadManifestUnchecked(const std::string& dir,
                                               const std::string& test_file,
                                               Manifest::Location location,
                                               int extra_flags,
                                               std::string* error) {
  return LoadManifestUnchecked(
      dir, test_file, location, extra_flags, std::string(), error);
}

scoped_refptr<Extension> LoadManifest(const std::string& dir,
                                      const std::string& test_file,
                                      Manifest::Location location,
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
  return LoadManifest(dir, test_file, Manifest::INVALID_LOCATION, extra_flags);
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

std::unique_ptr<extensions::ScopedCurrentChannel>
GetOverrideChannelForActionType(extensions::ActionInfo::Type action_type) {
  std::unique_ptr<extensions::ScopedCurrentChannel> channel;
  // The "action" key is currently restricted to trunk. Use a fake channel iff
  // we're testing that key, so that we still get multi-channel coverage for
  // browser and page actions.
  switch (action_type) {
    case extensions::ActionInfo::TYPE_ACTION:
      channel = std::make_unique<extensions::ScopedCurrentChannel>(
          version_info::Channel::UNKNOWN);
      break;
    case extensions::ActionInfo::TYPE_PAGE:
    case extensions::ActionInfo::TYPE_BROWSER:
      break;
  }
  return channel;
}

}  // namespace extension_test_util
