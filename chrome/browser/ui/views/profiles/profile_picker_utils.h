// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_

#include <memory>

namespace blink::mojom {
class WindowFeatures;
}

namespace content {
class WebContents;
}

class GURL;
class Profile;

// Opens the given `contents` as a 'Learn more' popup window with the given
// `target_url` and `window_features`.
//
// It should be used to open 'Learn more' pages in Profile Picker to avoid users
// freely browsing and abandoning the profile creation flow.
void OpenLearnMorePopup(Profile* profile,
                        std::unique_ptr<content::WebContents> contents,
                        const GURL& target_url,
                        const blink::mojom::WindowFeatures& window_features);

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_
