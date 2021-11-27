// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"

class AccessCodeCastDialog : public ui::WebDialogDelegate {
 public:
  explicit AccessCodeCastDialog(media_router::MediaCastMode cast_mode);
  ~AccessCodeCastDialog() override;
  AccessCodeCastDialog(const AccessCodeCastDialog&) = delete;
  AccessCodeCastDialog& operator=(const AccessCodeCastDialog&) = delete;
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
  raw_ptr<content::WebUI> webui_ = nullptr;
  media_router::MediaCastMode cast_mode_;
};

// The WebUI controller for chrome://access-code-cast.
class AccessCodeCastUI : public ui::MojoWebDialogUI,
                         public access_code_cast::mojom::PageHandlerFactory {
 public:
  explicit AccessCodeCastUI(content::WebUI* web_ui);
  ~AccessCodeCastUI() override;

  AccessCodeCastUI(const AccessCodeCastUI&) = delete;
  AccessCodeCastUI& operator=(const AccessCodeCastUI&) = delete;

  void BindInterface(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandlerFactory>
          receiver);

 private:
  // access_code_cast::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler)
      override;

  std::unique_ptr<AccessCodeCastHandler> page_handler_;
  mojo::Receiver<access_code_cast::mojom::PageHandlerFactory> factory_receiver_{
      this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
