// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_RESOURCE_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_RESOURCE_UTILS_H_

#include <string>
#include <utility>

namespace autofill {

// Returns the icon resource id corresponding to the |resource_name|.
int GetIconResourceID(const std::string& resource_name);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_UI_AUTOFILL_RESOURCE_UTILS_H_
