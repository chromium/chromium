// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_handler.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "components/media_router/browser/presentation/start_presentation_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "ui/web_dialogs/web_dialog_ui.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

class AccessCodeCastDialog : public ui::WebDialogDelegate,
                             public views::WidgetObserver {
 public:
  AccessCodeCastDialog(content::BrowserContext* context,
                       const media_router::CastModeSet& cast_mode_set,
                       content::WebContents* web_contents,
                       std::unique_ptr<media_router::StartPresentationContext>
                           start_presentation_context);
  ~AccessCodeCastDialog() override;
  AccessCodeCastDialog(const AccessCodeCastDialog&) = delete;
  AccessCodeCastDialog& operator=(const AccessCodeCastDialog&) = delete;

  static void Show(const media_router::CastModeSet& cast_mode_set,
                   content::WebContents* web_contents,
                   std::unique_ptr<media_router::StartPresentationContext>
                       start_presentation_context,
                   AccessCodeCastDialogOpenLocation open_location);
  // Show the access code dialog box for desktop mirroring.
  static void ShowForDesktopMirroring();

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

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
  bool ShouldShowCloseButton() const override;
  FrameKind GetWebDialogFrameKind() const override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  bool CheckMediaAccessPermission(content::RenderFrameHost* render_frame_host,
                                  const GURL& security_origin,
                                  blink::mojom::MediaStreamType type) override;

  static void Show(gfx::NativeView parent,
                   content::BrowserContext* context,
                   const media_router::CastModeSet& cast_mode_set,
                   content::WebContents* web_contents,
                   std::unique_ptr<media_router::StartPresentationContext>
                       start_presentation_context,
                   AccessCodeCastDialogOpenLocation open_location);

  // ObserveWidget must be called during dialog initialization so that we can
  // observe the dialog for activation state changes and close the dialog when
  // it loses focus.
  void ObserveWidget(views::Widget*);

  views::Widget* dialog_widget_ = nullptr;
  raw_ptr<content::WebUI> webui_ = nullptr;
  const raw_ptr<content::BrowserContext> context_;
  // Cast modes that should be attempted.
  const media_router::CastModeSet cast_mode_set_;
  const raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<media_router::StartPresentationContext>
      start_presentation_context_;
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

  // Set the set of modes that should be attempted when casting.
  virtual void SetCastModeSet(const media_router::CastModeSet& cast_mode_set);

  // This is the browser context that was used to launch the media router
  // dialog. This may be different than the context that was used to launch
  // this dialog.
  virtual void SetBrowserContext(content::BrowserContext* context);

  // The webcontents that were in focus when the media router dialog was
  // launched. May be null in the case of desktop casting.
  virtual void SetWebContents(content::WebContents* web_contents);

  virtual void SetStartPresentationContext(
      std::unique_ptr<media_router::StartPresentationContext>
          start_presentation_context);

 private:
  // access_code_cast::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler)
      override;

  std::unique_ptr<media_router::AccessCodeCastHandler> page_handler_;
  mojo::Receiver<access_code_cast::mojom::PageHandlerFactory> factory_receiver_{
      this};

  raw_ptr<content::BrowserContext> context_ = nullptr;
  media_router::CastModeSet cast_mode_set_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  std::unique_ptr<media_router::StartPresentationContext>
      start_presentation_context_;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_UI_H_
