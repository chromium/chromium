// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_FEATURES_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace webapps {
namespace features {

extern const base::Feature kAddToHomescreenMessaging;
extern const base::Feature kInstallableAmbientBadgeInfoBar;
extern const base::Feature kInstallableAmbientBadgeMessage;

}  // namespace features
}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_FEATURES_H_
