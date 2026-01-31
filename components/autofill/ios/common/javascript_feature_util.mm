// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/javascript_feature_util.h"

web::ContentWorld ContentWorldForAutofillJavascriptFeatures() {
  return web::ContentWorld::kIsolatedWorld;
}
