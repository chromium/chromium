// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/glic/glic_page_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "base/version_info/version_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_window_controller.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/glic/glic.mojom.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "ui/gfx/geometry/mojom/geometry.mojom.h"
#include "ui/gfx/geometry/size.h"

namespace glic {
class GlicWebClientHandler : public glic::mojom::WebClientHandler,
                             public GlicWindowController::StateObserver {
 public:
  explicit GlicWebClientHandler(
      content::BrowserContext* browser_context,
      mojo::PendingReceiver<glic::mojom::WebClientHandler> receiver)
      : profile_(Profile::FromBrowserContext(browser_context)),
        glic_service_(
            GlicKeyedServiceFactory::GetGlicKeyedService(browser_context)),
        pref_service_(profile_->GetPrefs()),
        receiver_(this, std::move(receiver)) {}

  ~GlicWebClientHandler() override { Uninstall(); }

  // glic::mojom::WebClientHandler implementation.
  void WebClientInitialized(
      ::mojo::PendingRemote<glic::mojom::WebClient> web_client) override {
    web_client_.Bind(std::move(web_client));
    web_client_.set_disconnect_handler(base::BindOnce(
        &GlicWebClientHandler::WebClientDisconnected, base::Unretained(this)));
    glic_service_->window_controller().AddStateObserver(this);
    web_client_->NotifyPanelStateChange(
        glic_service_->window_controller().GetPanelState().Clone());
    // Configure the pref_change_registrar to listen for changes to the prefs
    pref_change_registrar_.Init(pref_service_);
    pref_change_registrar_.Add(
        prefs::kGlicMicrophoneEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicGeolocationEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));
    pref_change_registrar_.Add(
        prefs::kGlicTabContextEnabled,
        base::BindRepeating(&GlicWebClientHandler::OnPrefChanged,
                            base::Unretained(this)));

    // Communicate initial permission values to web client.
    SendPermissionsToWebClient();
    installed_ = true;
  }

  void GetChromeVersion(GetChromeVersionCallback callback) override {
    std::move(callback).Run(version_info::GetVersion());
  }

  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t> window_id,
                 CreateTabCallback callback) override {
    glic_service_->CreateTab(url, open_in_background, window_id,
                             std::move(callback));
  }

  void ClosePanel() override { glic_service_->ClosePanel(); }

  void ResizeWidget(const gfx::Size& size,
                    ResizeWidgetCallback callback) override {
    std::optional<gfx::Size> actual_size = glic_service_->ResizePanel(size);
    if (!actual_size) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(actual_size);
  }

  void GetContextFromFocusedTab(
      bool include_inner_text,
      bool include_viewport_screenshot,
      GetContextFromFocusedTabCallback callback) override {
    glic_service_->GetContextFromFocusedTab(
        include_inner_text, include_viewport_screenshot, std::move(callback));
  }

  void SetPanelDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas,
      SetPanelDraggableAreasCallback callback) override {
    if (!draggable_areas.empty()) {
      glic_service_->SetPanelDraggableAreas(draggable_areas);

    } else {
      // Default to the top bar area of the panel.
      // TODO(cuianthony): Define panel dimensions constants in shared location.
      glic_service_->SetPanelDraggableAreas({{0, 0, 400, 80}});
    }
    std::move(callback).Run();
  }

  void SetMicrophonePermissionState(
      bool enabled,
      SetMicrophonePermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicMicrophoneEnabled, enabled);
    std::move(callback).Run();
  }

  void SetLocationPermissionState(
      bool enabled,
      SetLocationPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicGeolocationEnabled, enabled);
    std::move(callback).Run();
  }

  void SetTabContextPermissionState(
      bool enabled,
      SetTabContextPermissionStateCallback callback) override {
    pref_service_->SetBoolean(prefs::kGlicTabContextEnabled, enabled);
    std::move(callback).Run();
  }

  void GetUserProfileInfo(GetUserProfileInfoCallback callback) override {
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(profile_->GetPath());
    if (!entry) {
      std::move(callback).Run(nullptr);
      return;
    }

    auto result = mojom::UserProfileInfo::New();
    // TODO(crbug.com/382794680): Determine the correct size.
    gfx::Image icon = entry->GetAvatarIcon(512);
    if (!icon.IsEmpty()) {
      result->avatar_icon = icon.AsBitmap();
    }
    result->display_name = base::UTF16ToUTF8(entry->GetGAIAName());
    result->email = base::UTF16ToUTF8(entry->GetUserName());

    std::move(callback).Run(std::move(result));
  }

  // GlicWindowController::StateObserver implementation.
  void PanelStateChanged(const mojom::PanelState& panel_state) override {
    web_client_->NotifyPanelStateChange(panel_state.Clone());
  }

  void Uninstall() {
    if (!installed_) {
      return;
    }

    pref_change_registrar_.Reset();
    glic_service_->window_controller().RemoveStateObserver(this);
    installed_ = false;
  }

 private:
  void WebClientDisconnected() { Uninstall(); }

  void OnPrefChanged(const std::string& pref_name) {
    bool is_enabled = pref_service_->GetBoolean(pref_name);
    if (pref_name == prefs::kGlicMicrophoneEnabled) {
      web_client_->NotifyMicrophonePermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicGeolocationEnabled) {
      web_client_->NotifyLocationPermissionStateChanged(is_enabled);
    } else if (pref_name == prefs::kGlicTabContextEnabled) {
      web_client_->NotifyTabContextPermissionStateChanged(is_enabled);
    } else {
      DCHECK(false) << "Unknown Glic permission pref changed: " << pref_name;
    }
  }

  void SendPermissionsToWebClient() {
    web_client_->NotifyMicrophonePermissionStateChanged(
        pref_service_->GetBoolean(prefs::kGlicMicrophoneEnabled));
    web_client_->NotifyLocationPermissionStateChanged(
        pref_service_->GetBoolean(prefs::kGlicGeolocationEnabled));
    web_client_->NotifyTabContextPermissionStateChanged(
        pref_service_->GetBoolean(prefs::kGlicTabContextEnabled));
  }

  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<Profile> profile_;
  raw_ptr<GlicKeyedService> glic_service_;
  raw_ptr<PrefService> pref_service_;
  mojo::Receiver<glic::mojom::WebClientHandler> receiver_;
  mojo::Remote<glic::mojom::WebClient> web_client_;
  bool installed_ = false;
};

GlicPageHandler::GlicPageHandler(
    content::BrowserContext* browser_context,
    mojo::PendingReceiver<glic::mojom::PageHandler> receiver)
    : browser_context_(browser_context), receiver_(this, std::move(receiver)) {}

GlicPageHandler::~GlicPageHandler() = default;

void GlicPageHandler::CreateWebClient(
    ::mojo::PendingReceiver<glic::mojom::WebClientHandler>
        web_client_receiver) {
  web_client_handler_ = std::make_unique<GlicWebClientHandler>(
      browser_context_, std::move(web_client_receiver));
}

}  // namespace glic
