// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/manta/manta_service_factory.h"
#include "chrome/browser/manta/snapper_provider.h"
#include "chrome/browser/new_tab_page/modules/new_tab_page_modules.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"

CustomizeChromePageHandler::CustomizeChromePageHandler(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler,
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents,
    const std::vector<std::pair<const std::string, int>> module_id_names)
    : ntp_custom_background_service_(ntp_custom_background_service),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile_)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      module_id_names_(module_id_names),
      page_(std::move(pending_page)),
      receiver_(this, std::move(pending_page_handler)) {
  CHECK(ntp_custom_background_service_);
  CHECK(theme_service_);
  CHECK(ntp_background_service_);
  ntp_background_service_->AddObserver(this);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpModulesVisible,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpDisabledModules,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                          base::Unretained(this)));
  if (IsCartModuleEnabled()) {
    pref_change_registrar_.Add(
        prefs::kCartDiscountEnabled,
        base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                            base::Unretained(this)));
  }
  pref_change_registrar_.Add(
      ntp_prefs::kNtpUseMostVisitedTiles,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpShortcutsVisible,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());

  if (base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeWallpaperSearch) &&
      base::FeatureList::IsEnabled(features::kMantaService)) {
    manta_service_ = manta::MantaServiceFactory::GetForProfile(profile_);
    snapper_provider_ = manta_service_->CreateSnapperProvider();
  }
}

CustomizeChromePageHandler::~CustomizeChromePageHandler() {
  ntp_background_service_->RemoveObserver(this);
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
}

void CustomizeChromePageHandler::ScrollToSection(
    CustomizeChromeSection section) {
  last_requested_section_ = section;
  side_panel::mojom::CustomizeChromeSection mojo_section;
  switch (section) {
    case CustomizeChromeSection::kUnspecified:
      // Cannot scroll to unspecified section.
      return;
    case CustomizeChromeSection::kAppearance:
      mojo_section = side_panel::mojom::CustomizeChromeSection::kAppearance;
      break;
    case CustomizeChromeSection::kShortcuts:
      mojo_section = side_panel::mojom::CustomizeChromeSection::kShortcuts;
      break;
    case CustomizeChromeSection::kModules:
      mojo_section = side_panel::mojom::CustomizeChromeSection::kModules;
      break;
  }
  page_->ScrollToSection(mojo_section);
}

void CustomizeChromePageHandler::SetDefaultColor() {
  theme_service_->UseDeviceTheme(false);
  theme_service_->UseDefaultTheme();
}

void CustomizeChromePageHandler::SetFollowDeviceTheme(bool follow) {
  theme_service_->UseDeviceTheme(follow);
}

void CustomizeChromePageHandler::SetBackgroundImage(
    const std::string& attribution_1,
    const std::string& attribution_2,
    const GURL& attribution_url,
    const GURL& image_url,
    const GURL& thumbnail_url,
    const std::string& collection_id) {
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      image_url, thumbnail_url, attribution_1, attribution_2, attribution_url,
      collection_id);
}

void CustomizeChromePageHandler::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  // Only populating the |collection_id| turns on refresh daily which overrides
  // the the selected image.
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* thumbnail_url */ GURL(),
      /* attribution_line_1= */ "", /* attribution_line_2= */ "",
      /* action_url= */ GURL(), collection_id);
}

void CustomizeChromePageHandler::GetBackgroundCollections(
    GetBackgroundCollectionsCallback callback) {
  if (!ntp_background_service_ || background_collections_callback_) {
    std::move(callback).Run(
        std::vector<side_panel::mojom::BackgroundCollectionPtr>());
    return;
  }
  background_collections_request_start_time_ = base::TimeTicks::Now();
  background_collections_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionInfo();
}

void CustomizeChromePageHandler::GetBackgroundImages(
    const std::string& collection_id,
    GetBackgroundImagesCallback callback) {
  if (background_images_callback_) {
    std::move(background_images_callback_)
        .Run(std::vector<side_panel::mojom::CollectionImagePtr>());
  }
  if (!ntp_background_service_) {
    std::move(callback).Run(
        std::vector<side_panel::mojom::CollectionImagePtr>());
    return;
  }
  images_request_collection_id_ = collection_id;
  background_images_request_start_time_ = base::TimeTicks::Now();
  background_images_callback_ = std::move(callback);
  ntp_background_service_->FetchCollectionImageInfo(collection_id);
}

void CustomizeChromePageHandler::ChooseLocalCustomBackground(
    ChooseLocalCustomBackgroundCallback callback) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_) {
    std::move(callback).Run(false);
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents_));
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  file_types.extensions.resize(1);
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("jpeg"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("png"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("gif"));
  file_types.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_UPLOAD_IMAGE_FORMAT));
  DCHECK(!choose_local_custom_background_callback_);
  choose_local_custom_background_callback_ = std::move(callback);
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, std::u16string(),
      profile_->last_selected_directory(), &file_types, 0,
      base::FilePath::StringType(), web_contents_->GetTopLevelNativeWindow(),
      nullptr);
}

void CustomizeChromePageHandler::RemoveBackgroundImage() {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->ResetCustomBackgroundInfo();
  }
}

void CustomizeChromePageHandler::WallpaperSearchCallback(
    std::unique_ptr<EndpointResponse> response) {
  return;
}

void CustomizeChromePageHandler::GetWallpaperSearchBackground() {
  if (base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeWallpaperSearch) &&
      base::FeatureList::IsEnabled(features::kMantaService)) {
    const std::string json = R"({"data": ""})";
    snapper_provider_->Call(
        json,
        base::BindOnce(&CustomizeChromePageHandler::WallpaperSearchCallback,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CustomizeChromePageHandler::UpdateTheme() {
  if (ntp_custom_background_service_) {
    ntp_custom_background_service_->RefreshBackgroundIfNeeded();
  }
  auto theme = side_panel::mojom::Theme::New();
  auto custom_background =
      ntp_custom_background_service_
          ? ntp_custom_background_service_->GetCustomBackground()
          : absl::nullopt;
  auto background_image = side_panel::mojom::BackgroundImage::New();
  if (custom_background.has_value()) {
    background_image->url = custom_background->custom_background_url;
    background_image->snapshot_url =
        custom_background->custom_background_snapshot_url;
    background_image->is_uploaded_image = custom_background->is_uploaded_image;
    background_image->title =
        custom_background->custom_background_attribution_line_1;
    background_image->collection_id = custom_background->collection_id;
    background_image->daily_refresh_enabled =
        custom_background->daily_refresh_enabled;
  } else {
    background_image = nullptr;
  }
  theme->background_image = std::move(background_image);
  theme->follow_device_theme = theme_service_->UsingDeviceTheme();

  auto user_color = theme_service_->GetUserColor();
  // If a user has the GM3 flag enabled but a GM2 theme set they are in a limbo
  // state between the 2 theme types. We need to get the color of their theme
  // with GetAutogeneratedThemeColor still until they set a GM3 theme, use the
  // old way of detecting default, and use the old color tokens to keep an
  // accurate representation of what the user is seeing.
  if (features::IsChromeWebuiRefresh2023() && user_color.has_value()) {
    theme->background_color =
        web_contents_->GetColorProvider().GetColor(ui::kColorSysInversePrimary);
    if (user_color.value() != SK_ColorTRANSPARENT) {
      theme->foreground_color = theme->background_color;
    }
  } else {
    theme->background_color =
        web_contents_->GetColorProvider().GetColor(kColorNewTabPageBackground);
    if (!theme_service_->UsingDefaultTheme() &&
        !theme_service_->UsingSystemTheme()) {
      theme->foreground_color =
          web_contents_->GetColorProvider().GetColor(ui::kColorFrameActive);
    }
  }
  theme->background_managed_by_policy =
      ntp_custom_background_service_->IsCustomBackgroundDisabledByPolicy();
  if (theme_service_->UsingExtensionTheme()) {
    const extensions::Extension* theme_extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetByID(theme_service_->GetThemeID());
    if (theme_extension) {
      auto third_party_theme_info =
          side_panel::mojom::ThirdPartyThemeInfo::New();
      third_party_theme_info->id = theme_extension->id();
      third_party_theme_info->name = theme_extension->name();
      theme->third_party_theme_info = std::move(third_party_theme_info);
    }
  }
  page_->SetTheme(std::move(theme));
}

void CustomizeChromePageHandler::OpenChromeWebStore() {
  NavigateParams navigate_params(
      profile_, GURL("https://chrome.google.com/webstore?category=theme"),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::SHOW_WINDOW;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen",
                            NtpChromeWebStoreOpen::kAppearance);
}

void CustomizeChromePageHandler::OpenThirdPartyThemePage(
    const std::string& theme_id) {
  NavigateParams navigate_params(
      profile_, GURL("https://chrome.google.com/webstore/detail/" + theme_id),
      ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::SHOW_WINDOW;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen",
                            NtpChromeWebStoreOpen::kCollections);
}

void CustomizeChromePageHandler::SetMostVisitedSettings(
    bool custom_links_enabled,
    bool visible) {
  if (IsCustomLinksEnabled() != custom_links_enabled) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpUseMostVisitedTiles,
                                     !custom_links_enabled);
    LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE);
  }

  if (IsShortcutsVisible() != visible) {
    profile_->GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, visible);
    LogEvent(NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY);
  }
}

void CustomizeChromePageHandler::UpdateMostVisitedSettings() {
  page_->SetMostVisitedSettings(IsCustomLinksEnabled(), IsShortcutsVisible());
}

void CustomizeChromePageHandler::SetModulesVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
}

void CustomizeChromePageHandler::SetModuleDisabled(const std::string& module_id,
                                                   bool disabled) {
  ScopedListPrefUpdate update(profile_->GetPrefs(), prefs::kNtpDisabledModules);
  base::Value::List& list = update.Get();
  base::Value module_id_value(module_id);
  if (disabled) {
    if (!base::Contains(list, module_id_value)) {
      list.Append(std::move(module_id_value));
    }
  } else {
    list.EraseValue(module_id_value);
  }
}

void CustomizeChromePageHandler::UpdateModulesSettings() {
  std::vector<std::string> disabled_module_ids;
  for (const auto& id :
       profile_->GetPrefs()->GetList(prefs::kNtpDisabledModules)) {
    disabled_module_ids.push_back(id.GetString());
  }

  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  for (const auto& id_name_pair : module_id_names_) {
    auto module_settings = side_panel::mojom::ModuleSettings::New();
    module_settings->id = id_name_pair.first;
    module_settings->name = l10n_util::GetStringUTF8(id_name_pair.second);
    module_settings->enabled =
        !base::Contains(disabled_module_ids, module_settings->id);
    modules_settings.push_back(std::move(module_settings));
  }
  page_->SetModulesSettings(
      std::move(modules_settings),
      profile_->GetPrefs()->IsManagedPreference(prefs::kNtpModulesVisible),
      profile_->GetPrefs()->GetBoolean(prefs::kNtpModulesVisible));
}

void CustomizeChromePageHandler::UpdateScrollToSection() {
  ScrollToSection(last_requested_section_);
}

void CustomizeChromePageHandler::LogEvent(NTPLoggingEventType event) {
  switch (event) {
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeShortcutAction",
          CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_TYPE);
      break;
    case NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeShortcutAction",
          CustomizeShortcutAction::CUSTOMIZE_SHORTCUT_ACTION_TOGGLE_VISIBILITY);
      break;
    default:
      break;
  }
}

bool CustomizeChromePageHandler::IsCustomLinksEnabled() const {
  return !profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles);
}

bool CustomizeChromePageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

void CustomizeChromePageHandler::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnThemeChanged() {
  UpdateTheme();
}

void CustomizeChromePageHandler::OnCustomBackgroundImageUpdated() {
  OnThemeChanged();
}

void CustomizeChromePageHandler::OnNtpCustomBackgroundServiceShuttingDown() {
  ntp_custom_background_service_observation_.Reset();
  ntp_custom_background_service_ = nullptr;
}

void CustomizeChromePageHandler::OnCollectionInfoAvailable() {
  if (!background_collections_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_collections_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Collections.RequestLatency", duration);
  // Any response where no collections are returned is considered a failure.
  if (ntp_background_service_->collection_info().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
        duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Success",
        duration);
  }

  std::vector<side_panel::mojom::BackgroundCollectionPtr> collections;
  for (const auto& info : ntp_background_service_->collection_info()) {
    auto collection = side_panel::mojom::BackgroundCollection::New();
    collection->id = info.collection_id;
    collection->label = info.collection_name;
    collection->preview_image_url = GURL(info.preview_image_url);
    collections.push_back(std::move(collection));
  }
  std::move(background_collections_callback_).Run(std::move(collections));
}

void CustomizeChromePageHandler::OnCollectionImagesAvailable() {
  if (!background_images_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_images_request_start_time_;
  UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Images.RequestLatency", duration);
  // Any response where no images are returned is considered a failure.
  if (ntp_background_service_->collection_images().empty()) {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Failure", duration);
  } else {
    UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Success", duration);
  }

  std::vector<side_panel::mojom::CollectionImagePtr> images;
  if (ntp_background_service_->collection_images().empty()) {
    std::move(background_images_callback_).Run(std::move(images));
    return;
  }
  auto collection_id =
      ntp_background_service_->collection_images()[0].collection_id;
  for (const auto& info : ntp_background_service_->collection_images()) {
    DCHECK(info.collection_id == collection_id);
    auto image = side_panel::mojom::CollectionImage::New();
    image->attribution_1 = !info.attribution.empty() ? info.attribution[0] : "";
    image->attribution_2 =
        info.attribution.size() > 1 ? info.attribution[1] : "";
    image->attribution_url = info.attribution_action_url;
    image->image_url = info.image_url;
    image->preview_image_url = info.thumbnail_image_url;
    image->collection_id = collection_id;
    images.push_back(std::move(image));
  }
  std::move(background_images_callback_).Run(std::move(images));
}

void CustomizeChromePageHandler::OnNextCollectionImageAvailable() {}

void CustomizeChromePageHandler::OnNtpBackgroundServiceShuttingDown() {
  ntp_background_service_->RemoveObserver(this);
  ntp_background_service_ = nullptr;
}

void CustomizeChromePageHandler::FileSelected(const base::FilePath& path,
                                              int index,
                                              void* params) {
  DCHECK(choose_local_custom_background_callback_);
  if (ntp_custom_background_service_) {
    theme_service_->UseDefaultTheme();

    profile_->set_last_selected_directory(path.DirName());
    ntp_custom_background_service_->SelectLocalBackgroundImage(path);
  }
  select_file_dialog_ = nullptr;
  std::move(choose_local_custom_background_callback_).Run(true);
}

void CustomizeChromePageHandler::FileSelectionCanceled(void* params) {
  DCHECK(choose_local_custom_background_callback_);
  select_file_dialog_ = nullptr;

  std::move(choose_local_custom_background_callback_).Run(false);
}
