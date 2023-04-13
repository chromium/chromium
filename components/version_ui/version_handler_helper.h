// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_
#define COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_

#include "base/values.h"

namespace variations {
enum class SeedType;
}

namespace version_ui {

// Returns the variation seed type to be displayed on the chrome://version page.
// Returns empty for Regular seed which should not be shown.
std::string SeedTypeToUiString(variations::SeedType seed_type);

// Returns the list of variations to be displayed on the chrome:://version page.
base::Value::List GetVariationsList();

// Returns the variations information in command line format to be displayed on
// the chrome:://version page.
base::Value GetVariationsCommandLineAsValue();

}  // namespace version_ui

#endif  // COMPONENTS_VERSION_UI_VERSION_HANDLER_HELPER_H_
