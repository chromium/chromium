// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_COMMON_JAVASCRIPT_FEATURE_UTIL_H_
#define COMPONENTS_AUTOFILL_IOS_COMMON_JAVASCRIPT_FEATURE_UTIL_H_

#import "ios/web/public/js_messaging/content_world.h"

// Returns the content world to use for the Autofill javascript features.
web::ContentWorld ContentWorldForAutofillJavascriptFeatures();

#endif  // COMPONENTS_AUTOFILL_IOS_COMMON_JAVASCRIPT_FEATURE_UTIL_H_
