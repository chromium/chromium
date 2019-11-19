// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/common/manifest.h"

class GURL;

namespace extensions {
class Extension;
class ScopedCurrentChannel;
}

namespace extension_test_util {

// Helpers for loading manifests, |dir| is relative to chrome::DIR_TEST_DATA
// followed by "extensions".
scoped_refptr<extensions::Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags,
    const std::string& id,
    std::string* error);

scoped_refptr<extensions::Extension> LoadManifestUnchecked(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags,
    std::string* error);

scoped_refptr<extensions::Extension> LoadManifest(
    const std::string& dir,
    const std::string& test_file,
    extensions::Manifest::Location location,
    int extra_flags);

scoped_refptr<extensions::Extension> LoadManifest(const std::string& dir,
                                                  const std::string& test_file,
                                                  int extra_flags);

scoped_refptr<extensions::Extension> LoadManifestStrict(
    const std::string& dir,
    const std::string& test_file);

scoped_refptr<extensions::Extension> LoadManifest(const std::string& dir,
                                                  const std::string& test_file);

void SetGalleryUpdateURL(const GURL& new_url);

// Returns a ScopedCurrentChannel object to use in tests if one is necessary for
// the given |action_type| specified in the manifest. This will only return
// non-null if the "action" manifest key is used.
// TODO(https://crbug.com/893373): Remove this one the "action" key is launched
// to stable.
std::unique_ptr<extensions::ScopedCurrentChannel>
GetOverrideChannelForActionType(extensions::ActionInfo::Type action_type);

}  // namespace extension_test_util

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_TEST_UTIL_H_
