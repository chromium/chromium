// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/appearance_section.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/color/system_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace settings_search {

namespace {

using mojom::SettingId;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

class AppearanceSectionTest : public testing::Test {
 protected:
  AppearanceSectionTest() {
#if BUILDFLAG(IS_LINUX)
    SetTheme(ui::SystemTheme::kDefault);
#endif
  }

  void RegisterItems() {
    section_.reset();
    registry_.emplace();
    section_ = std::make_unique<AppearanceSection>(&*registry_, &profile_);
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

#if BUILDFLAG(IS_LINUX)
  void SetTheme(ui::SystemTheme system_theme, std::string_view theme_id = "") {
    PrefService* prefs = profile_.GetPrefs();
    prefs->SetInteger(prefs::kSystemTheme, std::to_underlying(system_theme));
    prefs->SetString(prefs::kCurrentThemeID, theme_id);
  }
#endif

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
  std::unique_ptr<AppearanceSection> section_;
};

TEST_F(AppearanceSectionTest, RegistersAppearanceItems) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(organizer_panel::kOrganizerPanel);
  RegisterItems();

  std::vector<SettingId> expected = {
      SettingId::kAppearance,         SettingId::kThemes,
      SettingId::kCustomizeToolbar,   SettingId::kColorSchemeMode,
      SettingId::kShowHomeButton,     SettingId::kShowBookmarksBar,
      SettingId::kTabStripPosition,   SettingId::kShowTabSearchButton,
      SettingId::kShowSavedTabGroups, SettingId::kAutoPinNewTabGroups,
      SettingId::kSidePanelPosition,  SettingId::kTabHoverCard,
      SettingId::kFontSize,           SettingId::kCustomizeFonts,
      SettingId::kPageZoom,           SettingId::kSplitViewDragAndDrop};
#if BUILDFLAG(IS_LINUX)
  // Depends on the Ozone backend running the test, so mirror the platform
  // property rather than force one branch.
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_server_side_window_decorations) {
    expected.push_back(SettingId::kShowWindowDecorations);
  }
#endif
  EXPECT_THAT(GetIds(), UnorderedElementsAreArray(expected));
}

TEST_F(AppearanceSectionTest, ItemMetadata) {
  RegisterItems();

  const SettingsSearchItem& page = GetItem(SettingId::kAppearance);
  EXPECT_EQ("/appearance", page.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_APPEARANCE),
            page.GetTitle());
  EXPECT_THAT(page.breadcrumbs(), IsEmpty());

  const SettingsSearchItem& home = GetItem(SettingId::kShowHomeButton);
  EXPECT_EQ("/appearance", home.route_path());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_HOME_BUTTON),
            home.GetTitle());
  EXPECT_THAT(home.breadcrumbs(),
              ElementsAre(l10n_util::GetStringUTF16(IDS_SETTINGS_APPEARANCE)));
  EXPECT_THAT(home.GetSynonyms(),
              ElementsAre(u"house", u"icon", u"toolbar", u"homepage", u"start",
                          u"url", u"shortcut"));

  EXPECT_EQ("/fonts", GetItem(SettingId::kCustomizeFonts).route_path());
}

TEST_F(AppearanceSectionTest, OrganizerPanelHidesTabGroupItems) {
  base::test::ScopedFeatureList feature_list{organizer_panel::kOrganizerPanel};
  RegisterItems();

  EXPECT_THAT(GetIds(), Not(Contains(AnyOf(SettingId::kShowSavedTabGroups,
                                           SettingId::kAutoPinNewTabGroups))));
}

TEST_F(AppearanceSectionTest, BookmarksBarTitle) {
  base::test::ScopedFeatureList feature_list{
      ntp_features::kNtpSimplificationBookmarkBar};
  RegisterItems();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_BOOKMARKS_BAR),
            GetItem(SettingId::kShowBookmarksBar).GetTitle());

  feature_list.Reset();
  feature_list.InitAndDisableFeature(
      ntp_features::kNtpSimplificationBookmarkBar);
  RegisterItems();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_SHOW_BOOKMARKS_BAR),
            GetItem(SettingId::kShowBookmarksBar).GetTitle());
}

TEST_F(AppearanceSectionTest, HoverCardTitle) {
  base::test::ScopedFeatureList feature_list{features::kTabHoverCardImages};
  RegisterItems();
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SETTINGS_HOVER_CARD_TITLE),
            GetItem(SettingId::kTabHoverCard).GetTitle());

  feature_list.Reset();
  feature_list.InitAndDisableFeature(features::kTabHoverCardImages);
  RegisterItems();
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_SETTINGS_SHOW_HOVER_CARD_MEMORY_USAGE_STANDALONE),
            GetItem(SettingId::kTabHoverCard).GetTitle());
}

#if BUILDFLAG(IS_CHROMEOS)
// ChromeOS guests get the Appearance page without some of its controls.
TEST_F(AppearanceSectionTest, GuestHidesControls) {
  user_manager()->LoginUser(user_manager()->AddGuestUser()->GetAccountId());
  RegisterItems();

  EXPECT_THAT(GetIds(), Contains(SettingId::kAppearance));
  EXPECT_THAT(
      GetIds(),
      Not(Contains(AnyOf(SettingId::kThemes, SettingId::kShowHomeButton,
                         SettingId::kShowBookmarksBar, SettingId::kTabHoverCard,
                         SettingId::kPageZoom))));
}
#else
// Desktop guests don't get the Appearance page at all.
TEST_F(AppearanceSectionTest, GuestRegistersNothing) {
  profile().SetGuestSession(true);
  RegisterItems();

  EXPECT_THAT(GetIds(), IsEmpty());
}
#endif

#if BUILDFLAG(IS_LINUX)
TEST_F(AppearanceSectionTest, ColorSchemeModeFollowsTheme) {
  SetTheme(ui::SystemTheme::kGtk);
  RegisterItems();
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kColorSchemeMode)));

  SetTheme(ui::SystemTheme::kDefault);
  EXPECT_THAT(GetIds(), Contains(SettingId::kColorSchemeMode));

  SetTheme(ui::SystemTheme::kQt);
  EXPECT_THAT(GetIds(), Not(Contains(SettingId::kColorSchemeMode)));

  SetTheme(ui::SystemTheme::kQt, "custom-theme-id");
  EXPECT_THAT(GetIds(), Contains(SettingId::kColorSchemeMode));
}
#endif

}  // namespace

}  // namespace settings_search
