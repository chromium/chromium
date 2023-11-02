// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_TOOLS_H_
#define COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_TOOLS_H_

#include <memory>
#include <vector>

namespace payments {

struct AndroidAppDescription;

// Moves each activity in the given |app| into its own AndroidAppDescription in
// |destination|, so the code can treat each PAY intent target as its own
// payment app.
//
// The function does not clear |destination|, so the results of multiple calls
// can be appended to the same |destination|, e.g., in a loop.
//
// The |destination| should not be null.
void SplitPotentiallyMultipleActivities(
    std::unique_ptr<AndroidAppDescription> app,
    std::vector<std::unique_ptr<AndroidAppDescription>>* destination);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_TOOLS_H_
