// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_CONFIG_READER_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_CONFIG_READER_H_

#include <optional>

#include "base/values.h"

namespace base {
class FilePath;
}

namespace fuchsia_component_support {

// Return a JSON dictionary read from the calling Component's config-data.
// All *.json files in the config-data directory are read, parsed, and merged
// into a single JSON dictionary value.
// Null is returned if no config-data exists for the Component.
// CHECK()s if one or more config files are malformed, or there are duplicate
// non-dictionary fields in different config files.
const std::optional<base::Value::Dict>& LoadPackageConfig();

// Used to test the implementation of LoadPackageConfig().
std::optional<base::Value::Dict> LoadConfigFromDirForTest(
    const base::FilePath& dir);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_CONFIG_READER_H_
