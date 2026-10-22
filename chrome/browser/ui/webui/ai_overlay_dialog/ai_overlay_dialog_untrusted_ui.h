// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ttc/core/buildflags.h"
#if BUILDFLAG(ENABLE_TTC_WEBUI)
#include "chrome/browser/ttc/core/eval_harness.mojom.h"
#endif
#include "chrome/browser/ui/webui/ai_overlay_dialog/ai_overlay_dialog.mojom.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

// TopChrome doesn't exist on Android, so inherit from the base WebUIController
// instead.
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/top_chrome/untrusted_top_chrome_web_ui_controller.h"
using AiOverlayDialogUntrustedUIBase = UntrustedTopChromeWebUIController;
#else
#include "ui/webui/untrusted_web_ui_controller.h"
using AiOverlayDialogUntrustedUIBase = ui::UntrustedWebUIController;
#endif

namespace ttc {

class AiOverlayDialogUntrustedUI;
class AiOverlayDialogPageHandler;
class AiOverlayTools;
#if BUILDFLAG(ENABLE_TTC_WEBUI)
class HarnessPageHandler;
#endif
class PageContextMonitor;

class AiOverlayDialogUntrustedUIConfig
    : public content::DefaultWebUIConfig<AiOverlayDialogUntrustedUI> {
 public:
  AiOverlayDialogUntrustedUIConfig();

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

class AiOverlayDialogUntrustedUI : public AiOverlayDialogUntrustedUIBase,
                                   ai_overlay_dialog::mojom::PageHandlerFactory
#if BUILDFLAG(ENABLE_TTC_WEBUI)
    ,
                                   ttc::mojom::HarnessPageHandlerFactory
#endif
{
 public:
  explicit AiOverlayDialogUntrustedUI(content::WebUI* web_ui);
  AiOverlayDialogUntrustedUI(const AiOverlayDialogUntrustedUI&) = delete;
  AiOverlayDialogUntrustedUI& operator=(const AiOverlayDialogUntrustedUI&) =
      delete;
  ~AiOverlayDialogUntrustedUI() override;

  void BindInterface(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandlerFactory>
          receiver);
  void BindInterface(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver);
#if BUILDFLAG(ENABLE_TTC_WEBUI)
  void BindInterface(
      mojo::PendingReceiver<ttc::mojom::HarnessPageHandlerFactory> receiver);
#endif

  // ai_overlay_dialog::mojom::PageHandlerFactor interface
  void CreatePageHandler(
      mojo::PendingReceiver<ai_overlay_dialog::mojom::PageHandler> receiver,
      mojo::PendingRemote<ai_overlay_dialog::mojom::Page> page,
      mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> tools)
      override;

#if BUILDFLAG(ENABLE_TTC_WEBUI)
  // ttc::mojom::HarnessPageHandlerFactory interface
  void CreatePageHandler(
      mojo::PendingReceiver<ttc::mojom::HarnessPageHandler> handler) override;
#endif

  PageContextMonitor* page_context_monitor() const {
    return page_context_monitor_.get();
  }

  AiOverlayTools* tools() const { return tools_.get(); }

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<AiOverlayDialogPageHandler> page_handler_;
#if BUILDFLAG(ENABLE_TTC_WEBUI)
  std::unique_ptr<HarnessPageHandler> harness_page_handler_;
#endif

  // `page_context_monitor_` must be declared before `tools_` so that `tools_`
  // (which holds a raw pointer to it) is destroyed first to prevent a dangling
  // pointer during destruction.
  std::unique_ptr<PageContextMonitor> page_context_monitor_;
  std::unique_ptr<AiOverlayTools> tools_;

  mojo::Receiver<ai_overlay_dialog::mojom::PageHandlerFactory>
      page_handler_factory_receiver_{this};
#if BUILDFLAG(ENABLE_TTC_WEBUI)
  mojo::Receiver<ttc::mojom::HarnessPageHandlerFactory>
      harness_page_handler_factory_receiver_{this};
#endif
};

}  // namespace ttc

#endif  // CHROME_BROWSER_UI_WEBUI_AI_OVERLAY_DIALOG_AI_OVERLAY_DIALOG_UNTRUSTED_UI_H_
