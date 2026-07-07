// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_

#include <string>
#include <utility>
#include <vector>

class GURL;
class Profile;
class ProfileAttributesEntry;
struct AccountInfo;

namespace gfx {
class ImageSkia;
}

namespace ui {
class ColorProvider;
class ImageModel;
}  // namespace ui

// LINT.IfChange(AiRingSpecs)
inline constexpr int kAiRingGapDip = 2;
inline constexpr int kAiRingThicknessDip = 3;
// LINT.ThenChange(//chrome/browser/resources/signin/profile_picker/profile_picker_shared.css:AiRingSpecs)

// Navigates to the Google Account page.
void NavigateToGoogleAccountPage(Profile* profile, const std::string& email);

// Returns true if account sync is paused, and a sync consent is present.
bool IsSyncPaused(Profile* profile);

// Returns true if there is an unconstented profile.
bool HasUnconstentedProfile(Profile* profile);

// Returns the number of browsers associated with |profile|.
// Note: For regular profiles this includes incognito sessions.
int CountBrowsersFor(Profile* profile);

// Returns the AccountInfo from the profile.
AccountInfo GetAccountInfoFromProfile(const Profile* profile);

// Returns the ProfileAttributesEntry from the profile.
ProfileAttributesEntry* GetProfileAttributesFromProfile(const Profile* profile);

// Returns the profile display name based off the profile attributes.
std::u16string GetProfileMenuDisplayName(
    ProfileAttributesEntry* profile_attributes);

// Returns all profile entries sorted by local profile name except for the
// current or omitted profiles.
std::vector<ProfileAttributesEntry*> GetAllOtherProfileEntriesForProfileSubMenu(
    const Profile* current_profile);

// Returns true if |command_id| identifies an other profile menu item.
bool IsOtherProfileCommand(int command_id);

// True if the UI may present an affordance to open `url` into an OTR context
// (e.g. a menu option to open the link in a new incognito window).
bool IsOpenLinkOTREnabled(Profile* source_profie, const GURL& url);

// Returns true if the AI subscription ring feature is enabled and the profile
// is eligible (has an active subscription tier > 0).
bool IsAiSubscriptionRingEnabled(Profile* profile);

// Returns the avatar image with the AI subscription ring.
gfx::ImageSkia AddAiRingToAvatar(const ui::ImageModel& avatar_image,
                                 const ui::ColorProvider& color_provider,
                                 int avatar_size,
                                 int gap_width = kAiRingGapDip,
                                 int ring_thickness = kAiRingThicknessDip);

#endif  // CHROME_BROWSER_UI_PROFILES_PROFILE_VIEW_UTILS_H_
