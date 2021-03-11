// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "build/buildflag.h"
#include "chrome/browser/ash/assistant/assistant_util.h"
#include "chrome/browser/ash/login/ui/oobe_dialog_size_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/views/chrome_web_dialog_view.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chromeos/assistant/buildflags.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"
#include "chromeos/services/assistant/public/cpp/features.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"

namespace chromeos {

namespace {

AssistantOptInDialog* g_dialog = nullptr;

constexpr int kCaptionBarHeight = 32;
constexpr int kDialogMargin = 48;
constexpr gfx::Size kDialogMaxSize = gfx::Size(768, 768);
constexpr gfx::Size kDialogMinSize = gfx::Size(544, 464);
constexpr gfx::Insets kDialogInsets =
    gfx::Insets(kDialogMargin + kCaptionBarHeight,
                kDialogMargin,
                kDialogMargin,
                kDialogMargin);

constexpr char kFlowTypeParamKey[] = "flow-type";
constexpr char kCaptionBarHeightParamKey[] = "caption-bar-height";
constexpr char kOobeDialogHeightParamKey[] = "oobe-dialog-height";
constexpr char kOobeDialogWidthParamKey[] = "oobe-dialog-width";

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  auto assistant_handler =
      std::make_unique<AssistantOptInFlowScreenHandler>(&js_calls_container_);
  assistant_handler_ptr_ = assistant_handler.get();
  web_ui->AddMessageHandler(std::move(assistant_handler));
  assistant_handler_ptr_->set_on_initialized(base::BindOnce(
      &AssistantOptInUI::Initialize, weak_factory_.GetWeakPtr()));
  assistant_handler_ptr_->SetupAssistantConnection();

  base::DictionaryValue localized_strings;
  assistant_handler_ptr_->GetLocalizedStrings(&localized_strings);

  OobeUI::AddOobeComponents(source, localized_strings);

  source->AddLocalizedStrings(localized_strings);
  source->UseStringsJs();
  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->AddResourcePath("assistant_logo.png", IDR_ASSISTANT_LOGO_PNG);
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);
  source->AddResourcePath("voice_match_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ANIMATION);
  source->AddResourcePath("voice_match_already_setup_animation.json",
                          IDR_ASSISTANT_VOICE_MATCH_ALREADY_SETUP_ANIMATION);
  source->AddBoolean("newLayoutEnabled",
                     chromeos::features::IsNewOobeLayoutEnabled());
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src blob: 'self';");
  source->DisableTrustedTypesCSP();
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Do not zoom for Assistant opt-in web contents.
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_ui->GetWebContents());
  DCHECK(zoom_map);
  zoom_map->SetZoomLevelForHost(web_ui->GetWebContents()->GetURL().host(), 0);
}

AssistantOptInUI::~AssistantOptInUI() = default;

void AssistantOptInUI::OnDialogClosed() {
  if (assistant_handler_ptr_) {
    assistant_handler_ptr_->OnDialogClosed();
  }
}

void AssistantOptInUI::Initialize() {
  js_calls_container_.ExecuteDeferredJSCalls(web_ui());
}

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show(
    ash::FlowType type,
    ash::AssistantSetup::StartAssistantOptInFlowCallback callback) {
#if !BUILDFLAG(ENABLE_CROS_LIBASSISTANT)
  std::move(callback).Run(false);
#else
  // Check Assistant allowed state.
  if (::assistant::IsAssistantAllowedForProfile(
          ProfileManager::GetActiveUserProfile()) !=
      chromeos::assistant::AssistantAllowedState::ALLOWED) {
    std::move(callback).Run(false);
    return;
  }

  // Check session state here to prevent timing issue -- session state might
  // have changed during the mojom calls to launch the opt-in dalog.
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    std::move(callback).Run(false);
    return;
  }
  if (g_dialog) {
    g_dialog->Focus();
    std::move(callback).Run(false);
    return;
  }
  g_dialog = new AssistantOptInDialog(type, std::move(callback));

  g_dialog->ShowSystemDialog();
#endif
}

// static
bool AssistantOptInDialog::BounceIfActive() {
  if (!g_dialog)
    return false;

  g_dialog->Focus();
  wm::AnimateWindow(g_dialog->dialog_window(),
                    wm::WINDOW_ANIMATION_TYPE_BOUNCE);
  return true;
}

AssistantOptInDialog::AssistantOptInDialog(
    ash::FlowType type,
    ash::AssistantSetup::StartAssistantOptInFlowCallback callback)
    : SystemWebDialogDelegate(CreateAssistantOptInURL(type), std::u16string()),
      callback_(std::move(callback)) {}

AssistantOptInDialog::~AssistantOptInDialog() {
  DCHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

void AssistantOptInDialog::AdjustWidgetInitParams(
    views::Widget::InitParams* params) {
  params->z_order = ui::ZOrderLevel::kNormal;
}

GURL AssistantOptInDialog::CreateAssistantOptInURL(ash::FlowType type) {
  GURL gurl(chrome::kChromeUIAssistantOptInURL);
  gurl = net::AppendQueryParameter(
      gurl, kFlowTypeParamKey, base::NumberToString(static_cast<int>(type)));
  gurl = net::AppendQueryParameter(gurl, kCaptionBarHeightParamKey,
                                   base::NumberToString(kCaptionBarHeight));
  gfx::Size size;
  GetDialogSize(&size);
  gurl = net::AppendQueryParameter(gurl, kOobeDialogHeightParamKey,
                                   base::NumberToString(size.height()));
  gurl = net::AppendQueryParameter(gurl, kOobeDialogWidthParamKey,
                                   base::NumberToString(size.width()));
  return gurl;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  auto bounds = display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  gfx::Size dialog_size;
  if (features::IsNewOobeLayoutEnabled()) {
    const bool is_horizontal = bounds.width() > bounds.height();
    dialog_size = CalculateOobeDialogSize(
        display::Screen::GetScreen()->GetPrimaryDisplay().size(),
        ash::ShelfConfig::Get()->shelf_size(), is_horizontal);
  } else {
    bounds.Inset(kDialogInsets);
    dialog_size = bounds.size();
    dialog_size.SetToMin(kDialogMaxSize);
    dialog_size.SetToMax(kDialogMinSize);
  }
  size->SetSize(dialog_size.width(), dialog_size.height());
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

void AssistantOptInDialog::OnDialogShown(content::WebUI* webui) {
  assistant_ui_ = static_cast<AssistantOptInUI*>(webui->GetController());
}

void AssistantOptInDialog::OnDialogClosed(const std::string& json_retval) {
  if (assistant_ui_)
    assistant_ui_->OnDialogClosed();

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const bool completed =
      prefs->GetBoolean(chromeos::assistant::prefs::kAssistantEnabled) &&
      (prefs->GetInteger(assistant::prefs::kAssistantConsentStatus) ==
       assistant::prefs::ConsentStatus::kActivityControlAccepted);
  std::move(callback_).Run(completed);
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

}  // namespace chromeos
