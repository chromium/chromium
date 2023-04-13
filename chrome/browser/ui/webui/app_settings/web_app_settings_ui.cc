// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/app_settings_resources.h"
#include "chrome/grit/app_settings_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/gfx/native_widget_types.h"

namespace {

void AddAppManagementStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"close", IDS_CLOSE},
      {"title", IDS_WEB_APP_SETTINGS_TITLE},
      {"appManagementAppInstalledByPolicyLabel",
       IDS_APP_MANAGEMENT_POLICY_APP_POLICY_STRING},
      {"appManagementFileHandlingHeader",
       IDS_APP_MANAGEMENT_FILE_HANDLING_HEADER},
      {"appManagementNotificationsLabel", IDS_APP_MANAGEMENT_NOTIFICATIONS},
      {"appManagementPermissionsLabel", IDS_APP_MANAGEMENT_PERMISSIONS},
      {"appManagementLocationPermissionLabel", IDS_APP_MANAGEMENT_LOCATION},
      {"appManagementMicrophonePermissionLabel", IDS_APP_MANAGEMENT_MICROPHONE},
      {"appManagementMorePermissionsLabel", IDS_APP_MANAGEMENT_MORE_SETTINGS},
      {"appManagementCameraPermissionLabel", IDS_APP_MANAGEMENT_CAMERA},
      {"appManagementUninstallLabel", IDS_APP_MANAGEMENT_UNINSTALL_APP},
      {"appManagementWindowModeLabel", IDS_APP_MANAGEMENT_WINDOW},
      {"appManagementRunOnOsLoginModeLabel",
       IDS_APP_MANAGEMENT_RUN_ON_OS_LOGIN},
      {"controlledSettingPolicy", IDS_CONTROLLED_SETTING_POLICY},
      {"fileHandlingOverflowDialogTitle",
       IDS_APP_MANAGEMENT_FILE_HANDLING_OVERFLOW_DIALOG_TITLE},
      {"fileHandlingSetDefaults",
       IDS_APP_MANAGEMENT_FILE_HANDLING_SET_DEFAULTS_LINK},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}

class WebAppSettingsWindowDelegate : public AppManagementPageHandler::Delegate {
 public:
  explicit WebAppSettingsWindowDelegate(Profile* profile) : profile_(profile) {}

  ~WebAppSettingsWindowDelegate() override = default;

  gfx::NativeWindow GetUninstallAnchorWindow() const override {
    return chrome::FindTabbedBrowser(profile_, false)
        ->window()
        ->GetNativeWindow();
  }

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;
};

}  // namespace

// static
std::unique_ptr<AppManagementPageHandler::Delegate>
WebAppSettingsUI::CreateAppManagementPageHandlerDelegate(Profile* profile) {
  return std::make_unique<WebAppSettingsWindowDelegate>(profile);
}

WebAppSettingsUI::WebAppSettingsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  // Set up the chrome://app-settings source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          profile, chrome::kChromeUIWebAppSettingsHost);

  AddAppManagementStrings(html_source);

  // Add required resources.
  webui::SetupWebUIDataSource(
      html_source,
      base::make_span(kAppSettingsResources, kAppSettingsResourcesSize),
      IDR_APP_SETTINGS_WEB_APP_SETTINGS_HTML);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  install_manager_observation_.Observe(&provider->install_manager());
}

void WebAppSettingsUI::BindInterface(
    mojo::PendingReceiver<app_management::mojom::PageHandlerFactory> receiver) {
  if (!app_management_page_handler_factory_) {
    auto window_delegate = std::make_unique<WebAppSettingsWindowDelegate>(
        Profile::FromWebUI(web_ui()));

    app_management_page_handler_factory_ =
        std::make_unique<AppManagementPageHandlerFactory>(
            Profile::FromWebUI(web_ui()), std::move(window_delegate));
  }
  app_management_page_handler_factory_->Bind(std::move(receiver));
}

void WebAppSettingsUI::OnWebAppUninstalled(
    const web_app::AppId& app_id,
    webapps::WebappUninstallSource uninstall_source) {
  auto* web_contents = web_ui()->GetWebContents();
  const web_app::AppId current_app_id =
      web_app::GetAppIdFromAppSettingsUrl(web_contents->GetURL());

  if (app_id == current_app_id)
    web_contents->ClosePage();
}

void WebAppSettingsUI::OnWebAppInstallManagerDestroyed() {
  install_manager_observation_.Reset();
}

WebAppSettingsUI::~WebAppSettingsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(WebAppSettingsUI)
