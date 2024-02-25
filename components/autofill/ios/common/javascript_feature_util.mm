// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/common/javascript_feature_util.h"

#import "base/feature_list.h"
#import "components/autofill/ios/common/features.h"

web::ContentWorld ContentWorldForAutofillJavascriptFeatures() {
  if (base::FeatureList::IsEnabled(kAutofillIsolatedWorldForJavascriptIos)) {
    return web::ContentWorld::kIsolatedWorld;
  }
  return web::ContentWorld::kPageContentWorld;
}
