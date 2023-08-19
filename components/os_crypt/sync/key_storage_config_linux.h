// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_CONFIG_LINUX_H_
#define COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_CONFIG_LINUX_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"

namespace os_crypt {

// A container for all the initialisation parameters for OSCrypt.
struct COMPONENT_EXPORT(OS_CRYPT) Config {
 public:
  Config();

  Config(const Config&) = delete;
  Config& operator=(const Config&) = delete;

  ~Config();

  // Force OSCrypt to use a specific linux password store.
  std::string store;
  // The product name to use for permission prompts.
  std::string product_name;
  // The application name to store the key under. For Chromium/Chrome builds
  // leave this unset and it will default correctly.  This config option is
  // for embedders to provide their application name in place of "Chromium".
  // Only used when the allow_runtime_configurable_key_storage feature is
  // enabled.
  std::string application_name;
  // Controls whether preference on using or ignoring backends is used.
  bool should_use_preference;
  // Preferences are stored in a separate file in the user data directory.
  base::FilePath user_data_path;
};

}  // namespace os_crypt

#endif  // COMPONENTS_OS_CRYPT_SYNC_KEY_STORAGE_CONFIG_LINUX_H_
