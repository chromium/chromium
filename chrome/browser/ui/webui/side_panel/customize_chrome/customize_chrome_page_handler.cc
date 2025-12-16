// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_page_handler.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/extensions/settings_api_helpers.h"
#include "chrome/browser/new_tab_page/modules/modules_constants.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_background_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/search/ntp_user_data_types.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/new_tab_footer/footer_controller.h"
#include "chrome/browser/ui/views/side_panel/customize_chrome/customize_chrome_utils.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_helper.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_section.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/ntp_tiles/tile_type.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider.h"
#include "ui/native_theme/native_theme.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace {

void OpenWebPage(Profile* profile, const GURL& url) {
  NavigateParams navigate_params(profile, url, ui::PAGE_TRANSITION_LINK);
  navigate_params.window_action = NavigateParams::WindowAction::kShowWindow;
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&navigate_params);
}

}  // namespace

// static
bool CustomizeChromePageHandler::IsSupported(
    NtpCustomBackgroundService* ntp_custom_background_service,
    Profile* profile) {
  if (!ntp_custom_background_service) {
    return false;
  }

  if (!ThemeServiceFactory::GetForProfile(profile)) {
    return false;
  }

  if (!NtpBackgroundServiceFactory::GetForProfile(profile)) {
    return false;
  }

  return true;
}

CustomizeChromePageHandler::CustomizeChromePageHandler(
    mojo::PendingReceiver<side_panel::mojom::CustomizeChromePageHandler>
        pending_page_handler,
    mojo::PendingRemote<side_panel::mojom::CustomizeChromePage> pending_page,
    NtpCustomBackgroundService* ntp_custom_background_service,
    content::WebContents* web_contents,
    const std::vector<ntp::ModuleIdDetail> module_id_details,
    std::optional<base::RepeatingCallback<void(const GURL&)>> open_url_callback)
    : ntp_custom_background_service_(ntp_custom_background_service),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ntp_background_service_(
          NtpBackgroundServiceFactory::GetForProfile(profile_)),
      template_url_service_(TemplateURLServiceFactory::GetForProfile(profile_)),
      theme_service_(ThemeServiceFactory::GetForProfile(profile_)),
      module_id_details_(module_id_details),
      browser_window_changed_subscription_(
          webui::RegisterBrowserWindowInterfaceChanged(
              web_contents_,
              base::BindRepeating(
                  &CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged,
                  base::Unretained(this)))),
      page_(std::move(pending_page)),
      receiver_(this, std::move(pending_page_handler)),
      open_url_callback_(open_url_callback.has_value()
                             ? open_url_callback.value()
                             : base::BindRepeating(&OpenWebPage, profile_)) {
  CHECK(IsSupported(ntp_custom_background_service_, profile_));

  ntp_background_service_->AddObserver(this);
  native_theme_observation_.Observe(ui::NativeTheme::GetInstanceForNativeUi());
  theme_service_observation_.Observe(theme_service_);

  OnBrowserWindowInterfaceChanged();

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kNtpModulesVisible,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpDisabledModules,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpCustomLinksVisible,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpEnterpriseShortcutsVisible,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpPersonalShortcutsVisible,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_prefs::kNtpShortcutsVisible,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      ntp_tiles::prefs::kEnterpriseShortcutsPolicyList,
      base::BindRepeating(
          &CustomizeChromePageHandler::UpdateMostVisitedSettings,
          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpHiddenModules,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateModulesSettings,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kNtpToolChipsVisible,
      base::BindRepeating(&CustomizeChromePageHandler::UpdateToolChipsSettings,
                          base::Unretained(this)));

  ntp_custom_background_service_observation_.Observe(
      ntp_custom_background_service_.get());

  if (template_url_service_) {
    template_url_service_->AddObserver(this);
  }
}

CustomizeChromePageHandler::~CustomizeChromePageHandler() {
  if (ntp_background_service_) {
    ntp_background_service_->RemoveObserver(this);
  }
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }
  if (template_url_service_) {
    template_url_service_->RemoveObserver(this);
  }
}

void CustomizeChromePageHandler::ScrollToSection(
    CustomizeChromeSection section) {
  last_requested_section_ = section;
  if (section == CustomizeChromeSection::kUnspecified) {
    // Cannot scroll to unspecified section.
    return;
  }
  page_->ScrollToSection(section);
}

void CustomizeChromePageHandler::AttachedTabStateUpdated(const GURL& url) {
  last_source_url_ = url;
  page_->AttachedTabStateUpdated(GetNewTabPageType(url));
}

bool CustomizeChromePageHandler::IsNtpManagedByThirdPartySearchEngine() const {
  return template_url_service_ &&
         template_url_service_->GetDefaultSearchProvider() &&
         !template_url_service_->GetDefaultSearchProvider()->HasGoogleBaseURLs(
             template_url_service_->search_terms_data());
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
  customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
}

void CustomizeChromePageHandler::SetDailyRefreshCollectionId(
    const std::string& collection_id) {
  // Only populating the |collection_id| turns on refresh daily which overrides
  // the the selected image.
  ntp_custom_background_service_->SetCustomBackgroundInfo(
      /* image_url */ GURL(), /* thumbnail_url */ GURL(),
      /* attribution_line_1= */ "", /* attribution_line_2= */ "",
      /* action_url= */ GURL(), collection_id);
  customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
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

void CustomizeChromePageHandler::GetReplacementCollectionPreviewImage(
    const std::string& collection_id,
    GetReplacementCollectionPreviewImageCallback callback) {
  callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(std::move(callback),
                                                         std::nullopt);
  if (!ntp_background_service_) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  ntp_background_service_->FetchReplacementCollectionPreviewImage(
      collection_id, std::move(callback));
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
    customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
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
          : std::nullopt;
  auto background_image = side_panel::mojom::BackgroundImage::New();
  if (custom_background.has_value()) {
    background_image->url = custom_background->custom_background_url;
    background_image->snapshot_url =
        custom_background->custom_background_snapshot_url;
    background_image->is_uploaded_image = custom_background->is_uploaded_image;
    background_image->local_background_id =
        custom_background->local_background_id;
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
  // If a user has a GM2 theme set they are in a limbo state between the 2 theme
  // types. We need to get the color of their theme with
  // GetAutogeneratedThemeColor still until they set a GM3 theme, use the old
  // way of detecting default, and use the old color tokens to keep an accurate
  // representation of what the user is seeing.
  if (user_color.has_value()) {
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

void CustomizeChromePageHandler::UpdateThemeEditable(bool is_theme_editable) {
  page_->SetThemeEditable(is_theme_editable);
}

void CustomizeChromePageHandler::OpenChromeWebStore() {
  open_url_callback_.Run(
      GURL("https://chromewebstore.google.com/category/themes"));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen",
                            NtpChromeWebStoreOpen::kAppearance);
}

void CustomizeChromePageHandler::OpenThirdPartyThemePage(
    const std::string& theme_id) {
  open_url_callback_.Run(GURL("https://chromewebstore.google.com/detail/" +
                              base::EscapePath(theme_id)));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen",
                            NtpChromeWebStoreOpen::kCollections);
}

void CustomizeChromePageHandler::OpenChromeWebStoreCategoryPage(
    side_panel::mojom::ChromeWebStoreCategory category) {
  std::string path;
  NtpChromeWebStoreOpen page;
  switch (category) {
    case side_panel::mojom::ChromeWebStoreCategory::kWorkflowPlanning:
      path = "extensions/productivity/workflow";
      page = NtpChromeWebStoreOpen::kWorkflowPlanningCategoryPage;
      break;
    case side_panel::mojom::ChromeWebStoreCategory::kShopping:
      path = "extensions/lifestyle/shopping";
      page = NtpChromeWebStoreOpen::kShoppingCategoryPage;
      break;
  }

  open_url_callback_.Run(GURL("https://chromewebstore.google.com/category/" +
                              path +
                              "?utm_source=chromeSidebarExtensionCards"));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen", page);
}

void CustomizeChromePageHandler::OpenChromeWebStoreCollectionPage(
    side_panel::mojom::ChromeWebStoreCollection collection) {
  std::string path;
  NtpChromeWebStoreOpen page;
  switch (collection) {
    case side_panel::mojom::ChromeWebStoreCollection::kWritingEssentials:
      path = "writing_essentials";
      page = NtpChromeWebStoreOpen::kWritingEssentialsCollectionPage;
      break;
  }

  open_url_callback_.Run(GURL("https://chromewebstore.google.com/collection/" +
                              path +
                              "?utm_source=chromeSidebarExtensionCards"));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen", page);
}

void CustomizeChromePageHandler::OpenChromeWebStoreHomePage() {
  open_url_callback_.Run(
      GURL("https://chromewebstore.google.com/"
           "?utm_source=chromeSidebarExtensionCards"));
  UMA_HISTOGRAM_ENUMERATION("NewTabPage.ChromeWebStoreOpen",
                            NtpChromeWebStoreOpen::kHomePage);
}

void CustomizeChromePageHandler::OpenNtpManagedByPage() {
  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    open_url_callback_.Run(
        net::AppendOrReplaceQueryParameter(GURL(chrome::kChromeUIExtensionsURL),
                                           "id", extension_managing_ntp->id()));
    return;
  }

  open_url_callback_.Run(GURL(chrome::kBrowserSettingsSearchEngineURL));
}

void CustomizeChromePageHandler::SetMostVisitedSettings(
    const std::vector<ntp_tiles::TileType>& types,
    bool visible,
    bool personal_shortcuts_visible) {
  std::set<ntp_tiles::TileType> types_set(types.begin(), types.end());
  std::set<ntp_tiles::TileType> current_tile_types = GetTileTypes();

  if ((base::Contains(current_tile_types, ntp_tiles::TileType::kCustomLinks) !=
           base::Contains(types_set, ntp_tiles::TileType::kCustomLinks) ||
       (base::Contains(current_tile_types, ntp_tiles::TileType::kTopSites) !=
        base::Contains(types_set, ntp_tiles::TileType::kTopSites)))) {
    UpdatePrefAndLogEvent(
        ntp_prefs::kNtpCustomLinksVisible,
        base::Contains(types_set, ntp_tiles::TileType::kCustomLinks),
        NTP_CUSTOMIZE_SHORTCUT_TOGGLE_TYPE);
  }

  if (base::Contains(current_tile_types,
                     ntp_tiles::TileType::kEnterpriseShortcuts) !=
      base::Contains(types_set, ntp_tiles::TileType::kEnterpriseShortcuts)) {
    // If enterprise shortcuts are disabled or the policy is not set, skip this
    // update.
    if (base::FeatureList::IsEnabled(ntp_tiles::kNtpEnterpriseShortcuts) &&
        !IsEnterpriseShortcutsEmpty()) {
      UpdatePrefAndLogEvent(
          ntp_prefs::kNtpEnterpriseShortcutsVisible,
          base::Contains(types_set, ntp_tiles::TileType::kEnterpriseShortcuts),
          NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_TOGGLE_VISIBILITY);
    }
  }

  if (IsShortcutsVisible() != visible) {
    UpdatePrefAndLogEvent(ntp_prefs::kNtpShortcutsVisible, visible,
                          NTP_CUSTOMIZE_SHORTCUT_TOGGLE_VISIBILITY);
  }

  if (IsPersonalShortcutsVisible() != personal_shortcuts_visible) {
    UpdatePrefAndLogEvent(ntp_prefs::kNtpPersonalShortcutsVisible,
                          personal_shortcuts_visible,
                          NTP_CUSTOMIZE_PERSONAL_SHORTCUT_TOGGLE_VISIBILITY);
  }
}

void CustomizeChromePageHandler::UpdateMostVisitedSettings() {
  std::vector<ntp_tiles::TileType> disabled_shortcuts;
  // If feature is not enabled or no enterprise shortcuts are set by policy,
  // hide the enterprise shortcuts option, but leave
  // the preference as is.
  if (!base::FeatureList::IsEnabled(ntp_tiles::kNtpEnterpriseShortcuts) ||
      IsEnterpriseShortcutsEmpty()) {
    disabled_shortcuts.push_back(ntp_tiles::TileType::kEnterpriseShortcuts);
  }
  auto tile_types = GetTileTypes();
  page_->SetMostVisitedSettings(
      {tile_types.begin(), tile_types.end()}, IsShortcutsVisible(),
      IsPersonalShortcutsVisible(), std::move(disabled_shortcuts));
}

void CustomizeChromePageHandler::OnBrowserWindowInterfaceChanged() {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }

  // TODO(webium): FooterController depends on BrowserView, but WebUIBrowser
  // doesn't have a BrowserView.
  if (webui_browser::IsWebUIBrowserEnabled()) {
    return;
  }

  footer_controller_observation_.Reset();
  auto* browser = webui::GetBrowserWindowInterface(web_contents_);
  if (!browser) {
    // TODO(crbug.com/378475391): NTP should always load into a WebContents
    // owned by a TabModel. Remove this once NTP loading has been restricted to
    // browser tabs only.
    return;
  }

  auto* footer_controller = browser->GetFeatures().new_tab_footer_controller();
  CHECK(footer_controller);
  footer_controller_observation_.Observe(footer_controller);
}

void CustomizeChromePageHandler::SetToolChipsVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpToolChipsVisible, visible);
}

void CustomizeChromePageHandler::UpdateToolChipsSettings() {
  page_->SetToolsSettings(
      profile_->GetPrefs()->GetBoolean(prefs::kNtpToolChipsVisible));
}

void CustomizeChromePageHandler::SetFooterVisible(bool visible) {
  profile_->GetPrefs()->SetBoolean(prefs::kNtpFooterVisible, visible);
}

void CustomizeChromePageHandler::UpdateFooterSettings() {
  auto management_notice_state =
      side_panel::mojom::ManagementNoticeState::New();
  management_notice_state->can_be_shown = false;
  management_notice_state->enabled_by_policy = false;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enterprise_util::BrowserManagementNoticeState state =
      enterprise_util::GetManagementNoticeStateForNTPFooter(profile_);
  switch (state) {
    case enterprise_util::BrowserManagementNoticeState::kNotApplicable:
      break;
    case enterprise_util::BrowserManagementNoticeState::kEnabled:
    case enterprise_util::BrowserManagementNoticeState::kDisabled:
      management_notice_state->can_be_shown = true;
      break;
    case enterprise_util::BrowserManagementNoticeState::kEnabledByPolicy:
      management_notice_state->can_be_shown = true;
      management_notice_state->enabled_by_policy = true;
      break;
  }
#endif

  page_->SetFooterSettings(
      profile_->GetPrefs()->GetBoolean(prefs::kNtpFooterVisible),
      profile_->GetPrefs()->GetBoolean(
          prefs::kNTPFooterExtensionAttributionEnabled),
      std::move(management_notice_state));
}

void CustomizeChromePageHandler::SetModulesVisible(bool visible) {
  DisableModuleAutoRemoval(profile_, ntp_modules::kAllModulesId);
  profile_->GetPrefs()->SetBoolean(prefs::kNtpModulesVisible, visible);
}

void CustomizeChromePageHandler::SetModuleDisabled(const std::string& module_id,
                                                   bool disabled) {
  DisableModuleAutoRemoval(profile_, module_id);
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

  std::vector<std::string> hidden_module_ids;
  for (const auto& id :
       profile_->GetPrefs()->GetList(prefs::kNtpHiddenModules)) {
    hidden_module_ids.push_back(id.GetString());
  }

  std::vector<side_panel::mojom::ModuleSettingsPtr> modules_settings;
  for (const auto& module_id_detail : module_id_details_) {
    auto module_settings = side_panel::mojom::ModuleSettings::New();
    module_settings->id = module_id_detail.id_;
    module_settings->name =
        l10n_util::GetStringUTF8(module_id_detail.name_message_id_);
    auto description_message_id = module_id_detail.description_message_id_;
    if (description_message_id.has_value()) {
      module_settings->description =
          l10n_util::GetStringUTF8(description_message_id.value());
    }
    module_settings->enabled =
        !base::Contains(disabled_module_ids, module_settings->id);
    module_settings->visible =
        !base::Contains(hidden_module_ids, module_settings->id);
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

void CustomizeChromePageHandler::UpdateAttachedTabState() {
  AttachedTabStateUpdated(last_source_url_);
}

void CustomizeChromePageHandler::UpdateNtpManagedByName() {
  std::string name;
  std::string description;

  // Check overriding extensions first.
  const extensions::Extension* extension_managing_ntp =
      extensions::GetExtensionOverridingNewTabPage(profile_);
  if (extension_managing_ntp) {
    name = extension_managing_ntp->short_name();
    description = l10n_util::GetStringUTF8(IDS_NTP_MANAGED_BY_EXTENSION);
  } else if (IsNtpManagedByThirdPartySearchEngine()) {
    name = base::UTF16ToUTF8(
        template_url_service_->GetDefaultSearchProvider()->short_name());
    description = l10n_util::GetStringUTF8(IDS_NTP_MANAGED_BY_SEARCH_ENGINE);
  }

  page_->NtpManagedByNameUpdated(name, description);
}

void CustomizeChromePageHandler::LogEvent(NTPLoggingEventType event) {
  switch (event) {
    case NTP_BACKGROUND_UPLOAD_CANCEL:
      base::RecordAction(base::UserMetricsAction(
          "NTPRicherPicker.Backgrounds.UploadCanceled"));
      break;
    case NTP_BACKGROUND_UPLOAD_DONE:
      base::RecordAction(base::UserMetricsAction(
          "NTPRicherPicker.Backgrounds.UploadConfirmed"));
      break;
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
    case NTP_CUSTOMIZE_PERSONAL_SHORTCUT_TOGGLE_VISIBILITY:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeShortcutAction",
          CustomizeShortcutAction::
              CUSTOMIZE_PERSONAL_SHORTCUT_ACTION_TOGGLE_VISIBILITY);
      break;
    case NTP_CUSTOMIZE_ENTERPRISE_SHORTCUT_TOGGLE_VISIBILITY:
      UMA_HISTOGRAM_ENUMERATION(
          "NewTabPage.CustomizeShortcutAction",
          CustomizeShortcutAction::
              CUSTOMIZE_ENTERPRISE_SHORTCUT_ACTION_TOGGLE_VISIBILITY);
      break;
    default:
      break;
  }
}

void CustomizeChromePageHandler::UpdatePrefAndLogEvent(
    const char* pref_name,
    bool new_value,
    NTPLoggingEventType event) {
  profile_->GetPrefs()->SetBoolean(pref_name, new_value);
  LogEvent(event);
}

std::set<ntp_tiles::TileType> CustomizeChromePageHandler::GetTileTypes() const {
  std::set<ntp_tiles::TileType> tile_types;
  if (IsEnterpriseShortcutsVisible()) {
    tile_types.insert(ntp_tiles::TileType::kEnterpriseShortcuts);
    // Skip adding personal shortcuts if enterprise shortcuts mixing is not
    // allowed.
    if (base::FeatureList::IsEnabled(ntp_tiles::kNtpEnterpriseShortcuts) &&
        !ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.Get()) {
      return tile_types;
    }
  }
  if (IsPersonalShortcutsVisible()) {
    tile_types.insert(
        profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpCustomLinksVisible)
            ? ntp_tiles::TileType::kCustomLinks
            : ntp_tiles::TileType::kTopSites);
  }
  return tile_types;
}

bool CustomizeChromePageHandler::IsShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(ntp_prefs::kNtpShortcutsVisible);
}

bool CustomizeChromePageHandler::IsPersonalShortcutsVisible() const {
  // Always return true if enterprise shortcuts feature is disabled,
  // enterprise shortcuts mixing is disabled, or no enterprise shortcuts are set
  // by policy.
  if (!base::FeatureList::IsEnabled(ntp_tiles::kNtpEnterpriseShortcuts) ||
      !ntp_tiles::kNtpEnterpriseShortcutsAllowMixingParam.Get() ||
      IsEnterpriseShortcutsEmpty()) {
    return true;
  }
  return profile_->GetPrefs()->GetBoolean(
      ntp_prefs::kNtpPersonalShortcutsVisible);
}

bool CustomizeChromePageHandler::IsEnterpriseShortcutsVisible() const {
  return profile_->GetPrefs()->GetBoolean(
      ntp_prefs::kNtpEnterpriseShortcutsVisible);
}

bool CustomizeChromePageHandler::IsEnterpriseShortcutsEmpty() const {
  return profile_->GetPrefs()
      ->GetList(ntp_tiles::prefs::kEnterpriseShortcutsPolicyList)
      .empty();
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

void CustomizeChromePageHandler::OnCollectionInfoAvailable() {
  if (!background_collections_callback_) {
    return;
  }

  base::TimeDelta duration =
      base::TimeTicks::Now() - background_collections_request_start_time_;
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Collections.RequestLatency", duration);
  // Any response where no collections are returned is considered a failure.
  if (ntp_background_service_->collection_info().empty()) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Collections.RequestLatency.Failure",
        duration);
  } else {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
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
  DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
      "NewTabPage.BackgroundService.Images.RequestLatency", duration);
  // Any response where no images are returned is considered a failure.
  if (ntp_background_service_->collection_images().empty()) {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
        "NewTabPage.BackgroundService.Images.RequestLatency.Failure", duration);
  } else {
    DEPRECATED_UMA_HISTOGRAM_MEDIUM_TIMES(
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

void CustomizeChromePageHandler::OnTemplateURLServiceChanged() {
  UpdateNtpManagedByName();
}

void CustomizeChromePageHandler::OnTemplateURLServiceShuttingDown() {
  CHECK(template_url_service_);
  template_url_service_->RemoveObserver(this);
  template_url_service_ = nullptr;
}

void CustomizeChromePageHandler::OnFooterVisibilityUpdated(bool visible) {
  if (!base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    return;
  }
  UpdateFooterSettings();
}

void CustomizeChromePageHandler::FileSelected(const ui::SelectedFileInfo& file,
                                              int index) {
  DCHECK(choose_local_custom_background_callback_);
  if (ntp_custom_background_service_) {
    // Use the default theme color if wallpaper search is disabled.
    // If wallpaper search is enabled, |ntp_custom_background_service_|
    // will handle setting the theme color.
    if (!base::FeatureList::IsEnabled(
            ntp_features::kCustomizeChromeWallpaperSearch) ||
        !base::FeatureList::IsEnabled(
            optimization_guide::features::kOptimizationGuideModelExecution)) {
      theme_service_->UseDefaultTheme();
    }

    profile_->set_last_selected_directory(file.path().DirName());
    ntp_custom_background_service_->SelectLocalBackgroundImage(file.path());
    customize_chrome::MaybeDisableExtensionOverridingNtp(profile_);
  }
  select_file_dialog_ = nullptr;
  LogEvent(NTP_BACKGROUND_UPLOAD_DONE);
  std::move(choose_local_custom_background_callback_).Run(true);
}

void CustomizeChromePageHandler::FileSelectionCanceled() {
  DCHECK(choose_local_custom_background_callback_);
  select_file_dialog_ = nullptr;
  LogEvent(NTP_BACKGROUND_UPLOAD_CANCEL);
  std::move(choose_local_custom_background_callback_).Run(false);
}

side_panel::mojom::NewTabPageType CustomizeChromePageHandler::GetNewTabPageType(
    const GURL& url) {
  if (NewTabPageUI::IsNewTabPageOrigin(url)) {
    return side_panel::mojom::NewTabPageType::kFirstPartyWebUI;
  } else if (ntp_footer::IsExtensionNtp(url, profile_)) {
    return side_panel::mojom::NewTabPageType::kExtension;
  } else if (NewTabPageThirdPartyUI::IsNewTabPageOrigin(url)) {
    return side_panel::mojom::NewTabPageType::kThirdPartyWebUI;
  } else if (IsNtpManagedByThirdPartySearchEngine()) {
    return side_panel::mojom::NewTabPageType::kThirdPartyRemote;
  } else if (NewTabUI::IsNewTab(url)) {
    return profile_->IsGuestSession()
               ? side_panel::mojom::NewTabPageType::kGuestMode
               : side_panel::mojom::NewTabPageType::kIncognito;
  }

  return side_panel::mojom::NewTabPageType::kNone;
}
