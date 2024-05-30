// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/attachment.h"
#include "chrome/browser/sharesheet/sharesheet_controller.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

class NearbySharingService;

namespace ui {
class ColorChangeHandler;
}  // namespace ui

namespace views {
class WebView;
}  // namespace views

namespace nearby_share {

class NearbyShareDialogUI;

// WebUIConfig for chrome://nearby
class NearbyShareDialogUIConfig
    : public content::DefaultWebUIConfig<NearbyShareDialogUI> {
 public:
  NearbyShareDialogUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUINearbyShareHost) {}

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUI controller for chrome://nearby.
class NearbyShareDialogUI : public ui::MojoWebUIController,
                            content::WebContentsDelegate {
 public:
  explicit NearbyShareDialogUI(content::WebUI* web_ui);
  NearbyShareDialogUI(const NearbyShareDialogUI&) = delete;
  NearbyShareDialogUI& operator=(const NearbyShareDialogUI&) = delete;
  ~NearbyShareDialogUI() override;

  void SetSharesheetController(sharesheet::SharesheetController* controller) {
    sharesheet_controller_ = controller;
  }
  void SetAttachments(std::vector<std::unique_ptr<Attachment>> attachments);
  void SetWebView(views::WebView* web_view);

  // Instantiates the implementor of the mojom::DiscoveryManager mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(mojo::PendingReceiver<mojom::DiscoveryManager> manager);
  // ui::MojoWebUIController
  void BindInterface(
      mojo::PendingReceiver<mojom::NearbyShareSettings> receiver);
  // Binds to the existing contacts manager instance owned by the nearby share
  // keyed service.
  void BindInterface(
      mojo::PendingReceiver<nearby_share::mojom::ContactManager> receiver);
  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // content::WebContentsDelegate:
  bool HandleKeyboardEvent(content::WebContents* source,
                           const input::NativeWebKeyboardEvent& event) override;
  void WebContentsCreated(content::WebContents* source_contents,
                          int opener_render_process_id,
                          int opener_render_frame_id,
                          const std::string& frame_name,
                          const GURL& target_url,
                          content::WebContents* new_contents) override;

 private:
  void HandleClose(const base::Value::List& args);

  // Search for a query parameter such as file, text, address, phone, or url,
  // then use it to populate an attachment, if found; otherwise, do nothing.
  // For text attachments, the parameter value is used as the text body. For
  // file attachments, the parameter value is used as a pipe-delimited list
  // of file paths.
  void SetAttachmentFromQueryParameter(const GURL& url);

  // A pointer to the Sharesheet controller is provided by
  // |NearbyShareAction::LaunchAction| when this WebUI controller is created. It
  // is used to close the Sharesheet in |HandleClose|.
  raw_ptr<sharesheet::SharesheetController, DanglingUntriaged>
      sharesheet_controller_ = nullptr;

  std::vector<std::unique_ptr<Attachment>> attachments_;
  raw_ptr<NearbySharingService> nearby_service_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace nearby_share

#endif  // CHROME_BROWSER_UI_WEBUI_NEARBY_SHARE_NEARBY_SHARE_DIALOG_UI_H_
