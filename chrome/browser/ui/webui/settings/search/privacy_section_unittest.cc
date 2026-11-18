// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/privacy_section.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

namespace settings_search {

namespace {

using mojom::SettingId;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

class PrivacySectionTest : public testing::Test {
 protected:
  void RegisterItems() {
    section_.reset();
    registry_.emplace();
    section_ = std::make_unique<PrivacySection>(&*registry_, &profile_);
  }

  std::vector<SettingId> GetIds() const {
    std::vector<SettingId> ids;
    for (const raw_ptr<SettingsSearchItem>& item : registry_->GetItems()) {
      ids.push_back(item->id());
    }
    return ids;
  }

  const SettingsSearchItem& GetItem(SettingId id) const {
    for (const raw_ptr<SettingsSearchItem>& item : registry_->GetItems()) {
      if (item->id() == id) {
        return *item;
      }
    }
    NOTREACHED();
  }

  TestingProfile& profile() { return profile_; }

#if BUILDFLAG(IS_CHROMEOS)
  ash::FakeChromeUserManager* user_manager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::optional<SettingsSearchRegistry> registry_{std::in_place};
  std::unique_ptr<PrivacySection> section_;
};

TEST_F(PrivacySectionTest, RegistersPrivacyItems) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {universal_optout::features::kUniversalOptOut},
      {universal_optout::features::kUniversalOptOutSettings});
  RegisterItems();

  // Keep in sync with `PrivacySection::RegisterSearchItems()`, in the order the
  // items are registered there.
  std::vector<SettingId> expected = {
      SettingId::kPrivacy,           SettingId::kSafetyCheck,
      SettingId::kClearBrowsingData, SettingId::kThirdPartyCookies,
      SettingId::kDoNotTrack,        SettingId::kThirdPartyCookiesAllowedSites,
  };
  EXPECT_THAT(GetIds(), UnorderedElementsAreArray(expected));
}

TEST_F(PrivacySectionTest, PageItemMetadata) {
  RegisterItems();

  const SettingsSearchItem& page = GetItem(SettingId::kPrivacy);
  EXPECT_EQ("/privacy", page.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY), page.GetTitle());
  EXPECT_THAT(page.breadcrumbs(), IsEmpty());
  EXPECT_THAT(page.GetSynonyms(),
              ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_PRIVACY)));
}

// Rows that open a subpage or dialog route there, not to the page they're on.
TEST_F(PrivacySectionTest, EntryRowsRouteToWhatTheyOpen) {
  RegisterItems();

  const SettingsSearchItem& safety_check = GetItem(SettingId::kSafetyCheck);
  EXPECT_EQ("/safetyCheck", safety_check.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SAFETY_HUB),
            safety_check.GetTitle());
  EXPECT_THAT(safety_check.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY)));
  EXPECT_THAT(safety_check.GetSynonyms(),
              ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_SAFETY_HUB)));

  const SettingsSearchItem& clear_data = GetItem(SettingId::kClearBrowsingData);
  EXPECT_EQ("/clearBrowserData", clear_data.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_CLEAR_BROWSING_DATA),
            clear_data.GetTitle());
  EXPECT_THAT(clear_data.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY)));
  EXPECT_THAT(
      clear_data.GetSynonyms(),
      ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_CLEAR_BROWSING_DATA)));

  const SettingsSearchItem& cookies = GetItem(SettingId::kThirdPartyCookies);
  EXPECT_EQ("/cookies", cookies.route_path());
  EXPECT_THAT(cookies.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY)));
  EXPECT_THAT(
      cookies.GetSynonyms(),
      ElementsAreArray(GetSynonyms(IDS_SETTINGS_TAG_THIRD_PARTY_COOKIES)));
}

TEST_F(PrivacySectionTest, CookiesSubpageItemMetadata) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {universal_optout::features::kUniversalOptOut},
      {universal_optout::features::kUniversalOptOutSettings});
  RegisterItems();

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_LABEL),
            GetItem(SettingId::kThirdPartyCookies).GetTitle());

  const SettingsSearchItem& do_not_track = GetItem(SettingId::kDoNotTrack);
  EXPECT_EQ("/cookies", do_not_track.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_ENABLE_DO_NOT_TRACK),
            do_not_track.GetTitle());
  EXPECT_THAT(do_not_track.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
                          l10n_util::GetStringUTF16(
                              IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_TITLE)));
  EXPECT_THAT(do_not_track.GetSynonyms(), IsEmpty());

  const SettingsSearchItem& allowed_sites =
      GetItem(SettingId::kThirdPartyCookiesAllowedSites);
  EXPECT_EQ("/cookies", allowed_sites.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_TRACKING_PROTECTION_SITES_ALLOWED_COOKIES_TITLE),
            allowed_sites.GetTitle());
  EXPECT_EQ(do_not_track.breadcrumbs(), allowed_sites.breadcrumbs());
  EXPECT_THAT(allowed_sites.GetSynonyms(), IsEmpty());
}

TEST_F(PrivacySectionTest, UniversalOptOutSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutSettings},
      {});
  profile().GetPrefs()->SetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled, true);
  RegisterItems();

  EXPECT_THAT(GetIds(),
              UnorderedElementsAre(SettingId::kPrivacy, SettingId::kSafetyCheck,
                                   SettingId::kClearBrowsingData,
                                   SettingId::kThirdPartyCookies,
                                   SettingId::kDoNotTrack,
                                   SettingId::kThirdPartyCookiesAllowedSites,
                                   SettingId::kUniversalOptOut));

  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_THIRD_PARTY_COOKIES_AND_SITE_DATA_LINK_ROW_LABEL),
            GetItem(SettingId::kThirdPartyCookies).GetTitle());

  const SettingsSearchItem& opt_out = GetItem(SettingId::kUniversalOptOut);
  EXPECT_EQ("/cookies", opt_out.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_UNIVERSAL_OPT_OUT_LABEL),
            opt_out.GetTitle());
  EXPECT_THAT(
      opt_out.breadcrumbs(),
      ElementsAre(
          l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
          l10n_util::GetStringUTF16(
              IDS_SETTINGS_THIRD_PARTY_COOKIES_AND_SITE_DATA_PAGE_TITLE)));
  EXPECT_THAT(opt_out.GetSynonyms(), IsEmpty());
  EXPECT_EQ(opt_out.breadcrumbs(),
            GetItem(SettingId::kDoNotTrack).breadcrumbs());
  EXPECT_EQ(opt_out.breadcrumbs(),
            GetItem(SettingId::kThirdPartyCookiesAllowedSites).breadcrumbs());
}

// The feature alone doesn't show the controls: the profile hasn't opted out,
// and unit tests have no `UniversalOptOutService` to make it eligible.
TEST_F(PrivacySectionTest, UniversalOptOutSettingsHiddenWithoutOptOut) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {universal_optout::features::kUniversalOptOut,
       universal_optout::features::kUniversalOptOutSettings},
      {});
  ASSERT_FALSE(profile().GetPrefs()->GetBoolean(
      universal_optout::prefs::kUniversalOptOutEnabled));
  RegisterItems();

  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kUniversalOptOut)));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_THIRD_PARTY_COOKIES_LINK_ROW_LABEL),
            GetItem(SettingId::kThirdPartyCookies).GetTitle());
  EXPECT_THAT(GetItem(SettingId::kDoNotTrack).breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_PRIVACY),
                          l10n_util::GetStringUTF16(
                              IDS_SETTINGS_THIRD_PARTY_COOKIES_PAGE_TITLE)));
  EXPECT_EQ(GetItem(SettingId::kDoNotTrack).breadcrumbs(),
            GetItem(SettingId::kThirdPartyCookiesAllowedSites).breadcrumbs());
}

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS guests get the Privacy and security page, without Safety Check.
TEST_F(PrivacySectionTest, GuestHidesSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {universal_optout::features::kUniversalOptOut},
      {universal_optout::features::kUniversalOptOutSettings});
  user_manager()->LoginUser(user_manager()->AddGuestUser()->GetAccountId());
  RegisterItems();

  EXPECT_THAT(GetIds(),
              UnorderedElementsAre(
                  SettingId::kPrivacy, SettingId::kClearBrowsingData,
                  SettingId::kThirdPartyCookies, SettingId::kDoNotTrack,
                  SettingId::kThirdPartyCookiesAllowedSites));
}
#else
// Desktop guests don't get the Privacy and security page at all.
TEST_F(PrivacySectionTest, GuestRegistersNothing) {
  profile().SetGuestSession(true);
  RegisterItems();

  EXPECT_THAT(GetIds(), IsEmpty());
}
#endif

}  // namespace

}  // namespace settings_search
