// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/whats_new/whats_new.mojom.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/resource/resource_scale_factor.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

namespace base {
class RefCountedMemory;
}

namespace content {
class WebUI;
}

class BrowserCommandHandler;
class PrefRegistrySimple;
class Profile;
class WhatsNewHandler;
class WhatsNewUI;

class WhatsNewUIConfig : public content::DefaultWebUIConfig<WhatsNewUI> {
 public:
  WhatsNewUIConfig();

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The Web UI controller for the chrome://whats-new page.
class WhatsNewUI : public ui::MojoWebUIController,
                   public whats_new::mojom::PageHandlerFactory,
                   public browser_command::mojom::CommandHandlerFactory,
                   content::WebContentsObserver {
 public:
  explicit WhatsNewUI(content::WebUI* web_ui);
  ~WhatsNewUI() override;

  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);

  // Instantiates the implementor of the
  // whats_new::mojom::PageHandlerFactory mojo interface.
  void BindInterface(
      mojo::PendingReceiver<whats_new::mojom::PageHandlerFactory> receiver);

  // Instantiates the implementor of the
  // browser_command::mojom::CommandHandlerFactory mojo interface.
  void BindInterface(
      mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
          pending_receiver);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;

  WhatsNewUI(const WhatsNewUI&) = delete;
  WhatsNewUI& operator=(const WhatsNewUI&) = delete;

 private:
  // whats_new::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<whats_new::mojom::Page> page,
      mojo::PendingReceiver<whats_new::mojom::PageHandler> receiver) override;

  std::unique_ptr<WhatsNewHandler> page_handler_;
  mojo::Receiver<whats_new::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  // browser_command::mojom::CommandHandlerFactory
  void CreateBrowserCommandHandler(
      mojo::PendingReceiver<browser_command::mojom::CommandHandler>
          pending_handler) override;

  std::unique_ptr<BrowserCommandHandler> command_handler_;
  mojo::Receiver<browser_command::mojom::CommandHandlerFactory>
      browser_command_factory_receiver_;
  raw_ptr<Profile> profile_;
  // Time the page started loading. Used for logging performance metrics.
  base::Time navigation_start_time_;
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_WHATS_NEW_WHATS_NEW_UI_H_
