// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_

#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/enterprise_casting/enterprise_casting.mojom.h"
#include "chrome/browser/ui/webui/enterprise_casting/enterprise_casting_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"

class EnterpriseCastingDialog : public ui::WebDialogDelegate {
 public:
  explicit EnterpriseCastingDialog(media_router::MediaCastMode cast_mode);
  ~EnterpriseCastingDialog() override;
  EnterpriseCastingDialog(const EnterpriseCastingDialog&) = delete;
  EnterpriseCastingDialog& operator=(const EnterpriseCastingDialog&) = delete;
  static void Show(media_router::MediaCastMode mode =
                       media_router::MediaCastMode::DESKTOP_MIRROR);

 private:
  ui::ModalType GetDialogModalType() const override;
  std::u16string GetDialogTitle() const override;
  GURL GetDialogContentURL() const override;
  void GetWebUIMessageHandlers(
      std::vector<content::WebUIMessageHandler*>* handlers) const override;
  void GetDialogSize(gfx::Size* size) const override;
  std::string GetDialogArgs() const override;
  void OnDialogShown(content::WebUI* webui) override;
  void OnDialogClosed(const std::string& json_retval) override;
  void OnCloseContents(content::WebContents* source,
                       bool* out_close_dialog) override;
  bool ShouldShowDialogTitle() const override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;
  content::WebUI* webui_ = nullptr;
  media_router::MediaCastMode cast_mode_;
};

// The WebUI controller for chrome://enterprise-casting.
class EnterpriseCastingUI
    : public ui::MojoWebDialogUI,
      public enterprise_casting::mojom::PageHandlerFactory {
 public:
  explicit EnterpriseCastingUI(content::WebUI* web_ui);
  ~EnterpriseCastingUI() override;

  EnterpriseCastingUI(const EnterpriseCastingUI&) = delete;
  EnterpriseCastingUI& operator=(const EnterpriseCastingUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandlerFactory>
          receiver);

 private:
  // enterprise_casting::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<enterprise_casting::mojom::Page> page,
      mojo::PendingReceiver<enterprise_casting::mojom::PageHandler>
          page_handler) override;

  std::unique_ptr<EnterpriseCastingHandler> page_handler_;
  mojo::Receiver<enterprise_casting::mojom::PageHandlerFactory>
      factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ENTERPRISE_CASTING_ENTERPRISE_CASTING_UI_H_
