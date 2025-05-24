// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_
#define COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_

#import "base/feature_list.h"

namespace password_manager::features {

// Features that are exclusive to ios and doesn't touch any cross-platform
// module go here.

// Enables the stateless FillData flow on iOS. When enabled,
// the AccountSelectFillData doesn't rely on intermediate steps anymore to
// handle the different steps in the FillData flow (i.e. you don't need to
// retrieve suggestions before filling them). This allow handling concurrent
// form suggestions retrieval smoothly, hence fully supporting the stateless
// FormSuggestionController, end to end.
BASE_DECLARE_FEATURE(kIOSStatelessFillDataFlow);

}  // namespace password_manager::features

#endif  // COMPONENTS_PASSWORD_MANAGER_IOS_FEATURES_H_
