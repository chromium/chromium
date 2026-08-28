// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_

#include <memory>
#include <optional>

#include "chrome/browser/ui/profiles/profile_picker.h"

namespace blink::mojom {
class WindowFeatures;
}

namespace content {
class WebContents;
}

namespace signin {
class IdentityManager;
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

// Computes the skip reason for the First Run experience, if any. Returns
// std::nullopt if the First Run experience should proceed.
std::optional<ProfilePicker::FirstRunFinishReason> ComputeFirstRunSkipReason(
    Profile& profile,
    signin::IdentityManager& identity_manager);

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_PROFILE_PICKER_UTILS_H_
