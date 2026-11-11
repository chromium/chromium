// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/search/appearance_section.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_item.h"
#include "chrome/browser/ui/webui/settings/search/settings_search_registry.h"
#include "chrome/browser/ui/webui/settings/search/settings_section.h"
#include "chrome/browser/ui/webui/settings/settings_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/search/ntp_features.h"

#if BUILDFLAG(IS_LINUX)
#include "base/functional/bind.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "ui/color/system_theme.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace settings_search {

namespace {

using mojom::SettingId;

constexpr int kAppearanceBreadcrumbs[] = {IDS_SETTINGS_APPEARANCE};

// Makes an item for a control on the Appearance page, or on `sub_page`.
std::unique_ptr<SettingsSearchItem> MakeItem(
    SettingId id,
    int title_id,
    std::optional<int> synonyms_id = std::nullopt,
    std::string_view sub_page = chrome::kAppearanceSubPage) {
  return MakeSearchItem(id, sub_page, title_id, kAppearanceBreadcrumbs,
                        synonyms_id);
}

std::unique_ptr<SettingsSearchItem> MakeColorSchemeModeItem() {
  return MakeItem(SettingId::kColorSchemeMode, IDS_SETTINGS_COLOR_SCHEME_MODE,
                  IDS_SETTINGS_TAG_COLOR_SCHEME_MODE);
}

}  // namespace

AppearanceSection::AppearanceSection(SettingsSearchRegistry* registry,
                                     Profile* profile)
    : SettingsSection(registry), profile_(profile) {
  CHECK(profile_);
  RegisterSearchItems();
}

AppearanceSection::~AppearanceSection() = default;

void AppearanceSection::RegisterSearchItems() {
  // Mirrors page_visibility.ts: guests don't get the Appearance page on
  // desktop, and on ChromeOS they don't get some of its controls.
  const bool is_guest = settings_utils::IsGuest(profile_);
#if !BUILDFLAG(IS_CHROMEOS)
  if (is_guest) {
    return;
  }
#endif
  SettingsSearchRegistry* registry = this->registry();

  registry->AddItem(
      MakeSearchItem(SettingId::kAppearance, chrome::kAppearanceSubPage,
                     IDS_SETTINGS_APPEARANCE, /*breadcrumb_ids=*/{},
                     IDS_SETTINGS_TAG_APPEARANCE));
  registry->AddItem(MakeItem(SettingId::kCustomizeToolbar,
                             IDS_SETTINGS_CUSTOMIZE_TOOLBAR,
                             IDS_SETTINGS_TAG_CUSTOMIZE_TOOLBAR));
  registry->AddItem(MakeItem(SettingId::kTabStripPosition,
                             IDS_SETTINGS_TAB_STRIP_POSITION,
                             IDS_SETTINGS_TAG_TAB_STRIP_POSITION));
  registry->AddItem(MakeItem(SettingId::kShowTabSearchButton,
                             IDS_SETTINGS_SHOW_TAB_SEARCH_BUTTON));
  // Mirrors loadTimeData "showOrganizerPanelEnabled".
  if (!organizer_panel::IsOrganizerPanelFeatureEnabled()) {
    registry->AddItem(MakeItem(SettingId::kShowSavedTabGroups,
                               IDS_SETTINGS_SHOW_TAB_GROUPS_IN_BOOKMARKS_BAR));
    registry->AddItem(MakeItem(SettingId::kAutoPinNewTabGroups,
                               IDS_SETTINGS_AUTO_PIN_NEW_TAB_GROUPS));
  }
  registry->AddItem(MakeItem(SettingId::kSidePanelPosition,
                             IDS_SETTINGS_SIDE_PANEL_POSITION,
                             IDS_SETTINGS_TAG_SIDE_PANEL_POSITION));
  registry->AddItem(
      MakeItem(SettingId::kFontSize, IDS_SETTINGS_FONT_SIZE_LABEL));
  registry->AddItem(
      MakeItem(SettingId::kCustomizeFonts, IDS_SETTINGS_CUSTOMIZE_FONTS,
               /*synonyms_id=*/std::nullopt, chrome::kFontsSubPage));
  registry->AddItem(MakeItem(SettingId::kSplitViewDragAndDrop,
                             IDS_SETTINGS_ALLOW_SPLIT_VIEW_DRAG_AND_DROP));

  if (!is_guest) {
    registry->AddItem(MakeItem(SettingId::kThemes, IDS_SETTINGS_THEMES,
                               IDS_SETTINGS_TAG_THEME));
    registry->AddItem(MakeItem(SettingId::kShowHomeButton,
                               IDS_SETTINGS_SHOW_HOME_BUTTON,
                               IDS_SETTINGS_TAG_SHOW_HOME_BUTTON));
    // Mirrors loadTimeData "ntpSimplificationBookmarksBarEnabled".
    registry->AddItem(MakeItem(SettingId::kShowBookmarksBar,
                               base::FeatureList::IsEnabled(
                                   ntp_features::kNtpSimplificationBookmarkBar)
                                   ? IDS_SETTINGS_BOOKMARKS_BAR
                                   : IDS_SETTINGS_SHOW_BOOKMARKS_BAR,
                               IDS_SETTINGS_TAG_SHOW_BOOKMARKS_BAR));
    // Mirrors loadTimeData "showHoverCardImagesOption".
    registry->AddItem(
        MakeItem(SettingId::kTabHoverCard,
                 base::FeatureList::IsEnabled(features::kTabHoverCardImages)
                     ? IDS_SETTINGS_HOVER_CARD_TITLE
                     : IDS_SETTINGS_SHOW_HOVER_CARD_MEMORY_USAGE_STANDALONE));
    registry->AddItem(MakeItem(SettingId::kPageZoom,
                               IDS_SETTINGS_PAGE_ZOOM_LABEL,
                               IDS_SETTINGS_TAG_PAGE_ZOOM));
  }

#if BUILDFLAG(IS_LINUX)
  // Mirrors loadTimeData "showCustomChromeFrame".
  if (ui::OzonePlatform::GetInstance()
          ->GetPlatformRuntimeProperties()
          .supports_server_side_window_decorations) {
    registry->AddItem(MakeItem(SettingId::kShowWindowDecorations,
                               IDS_SHOW_WINDOW_DECORATIONS));
  }
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCurrentThemeID,
      base::BindRepeating(&AppearanceSection::UpdateColorSchemeModeItem,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kSystemTheme,
      base::BindRepeating(&AppearanceSection::UpdateColorSchemeModeItem,
                          base::Unretained(this)));
  UpdateColorSchemeModeItem();
#else
  registry->AddItem(MakeColorSchemeModeItem());
#endif
}

#if BUILDFLAG(IS_LINUX)
void AppearanceSection::UpdateColorSchemeModeItem() {
  const PrefService* prefs = profile_->GetPrefs();
  const auto system_theme =
      static_cast<ui::SystemTheme>(prefs->GetInteger(prefs::kSystemTheme));
  // Mirrors showColorSchemeMode_() in appearance_page.ts.
  const bool shown = !prefs->GetString(prefs::kCurrentThemeID).empty() ||
                     (system_theme != ui::SystemTheme::kGtk &&
                      system_theme != ui::SystemTheme::kQt);
  if (shown == color_scheme_mode_shown_) {
    return;
  }
  color_scheme_mode_shown_ = shown;
  if (!shown) {
    registry()->RemoveItem(SettingId::kColorSchemeMode);
    return;
  }
  registry()->AddItem(MakeColorSchemeModeItem());
}
#endif

}  // namespace settings_search
