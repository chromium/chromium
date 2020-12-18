// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_
#define COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_

namespace base {
struct Feature;
}  // namespace base

namespace browser_ui {

// Enables messaging in the site settings UI to tell users notifications are
// disabled for the entire app
extern const base::Feature kAppNotificationStatusMessaging;
// Enables toggles and slash through diabled icons for content settings.
extern const base::Feature kActionableContentSettings;

}  // namespace browser_ui

#endif  // COMPONENTS_BROWSER_UI_SITE_SETTINGS_ANDROID_FEATURES_H_
