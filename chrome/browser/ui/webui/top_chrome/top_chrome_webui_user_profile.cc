// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_user_profile.h"

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"

namespace webui {

namespace {

// Holds the user profile of a WebContents created in TopChromeWebUIProfile.
class TopChromeWebUIUserProfileData
    : public content::WebContentsUserData<TopChromeWebUIUserProfileData> {
 public:
  TopChromeWebUIUserProfileData(const TopChromeWebUIUserProfileData&) = delete;
  TopChromeWebUIUserProfileData& operator=(
      const TopChromeWebUIUserProfileData&) = delete;
  ~TopChromeWebUIUserProfileData() override = default;

  Profile* user_profile() const { return user_profile_.get(); }

 private:
  friend class content::WebContentsUserData<TopChromeWebUIUserProfileData>;

  TopChromeWebUIUserProfileData(content::WebContents* web_contents,
                                Profile* user_profile)
      : content::WebContentsUserData<TopChromeWebUIUserProfileData>(
            *web_contents),
        user_profile_(user_profile->GetWeakPtr()) {}

  const base::WeakPtr<Profile> user_profile_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(TopChromeWebUIUserProfileData);

}  // namespace

void SetUserProfile(content::WebContents* web_contents, Profile* user_profile) {
  CHECK(Profile::FromBrowserContext(web_contents->GetBrowserContext())
            ->IsTopChromeWebUIProfile());
  CHECK(user_profile);
  // The user profile of a WebContents doesn't change.
  CHECK(!TopChromeWebUIUserProfileData::FromWebContents(web_contents));
  TopChromeWebUIUserProfileData::CreateForWebContents(web_contents,
                                                      user_profile);
}

Profile* GetUserProfile(content::WebContents* web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile->IsTopChromeWebUIProfile()) {
    return profile;
  }
  auto* data = TopChromeWebUIUserProfileData::FromWebContents(web_contents);
  return data ? data->user_profile() : nullptr;
}

}  // namespace webui
