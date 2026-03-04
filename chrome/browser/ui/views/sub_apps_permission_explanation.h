// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SUB_APPS_PERMISSION_EXPLANATION_H_
#define CHROME_BROWSER_UI_VIEWS_SUB_APPS_PERMISSION_EXPLANATION_H_

#include <optional>
#include <string>

namespace content {
class WebContents;
}

// When the app in `web_contents` is an IWA sub app or a parent IWA with sub
// apps, this returns a string explaining that sub apps share permissions with
// the parent IWA app. Otherwise returns `std::nullopt`.
std::optional<std::u16string> GetSubAppsPermissionExplanation(
    content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_SUB_APPS_PERMISSION_EXPLANATION_H_
