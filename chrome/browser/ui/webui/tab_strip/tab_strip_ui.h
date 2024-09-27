// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/thumbnail_tracker.h"
#include "chrome/browser/ui/webui/webui_load_timer.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"

namespace ui {
class ColorChangeHandler;
}  // namespace ui

class Browser;
class TabStripPageHandler;
class TabStripUIEmbedder;

// These data types must be in all lowercase.
constexpr char16_t kWebUITabIdDataType[] = u"application/vnd.chromium.tab";
constexpr char16_t kWebUITabGroupIdDataType[] =
    u"application/vnd.chromium.tabgroup";

class TabStripUI;

class TabStripUIConfig : public content::DefaultWebUIConfig<TabStripUI> {
 public:
  TabStripUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUITabStripHost) {}
};

// The WebUI version of the tab strip in the browser. It is currently only
// supported on ChromeOS in tablet mode.
class TabStripUI : public ui::MojoWebUIController,
                   public tab_strip::mojom::PageHandlerFactory {
 public:
  explicit TabStripUI(content::WebUI* web_ui);

  TabStripUI(const TabStripUI&) = delete;
  TabStripUI& operator=(const TabStripUI&) = delete;

  ~TabStripUI() override;

  // Instantiates the implementor of the mojom::PageHandlerFactory mojo
  // interface passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<tab_strip::mojom::PageHandlerFactory> receiver);

  // Instantiates the implementor of the mojom::PageHandler mojo interface
  // passing the pending receiver that will be internally bound.
  void BindInterface(
      mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
          receiver);

  // Initialize TabStripUI with its embedder and the Browser it's running in.
  // Must be called exactly once. The WebUI won't work until this is called.
  // |Deinitialize| is called during |embedder|'s destructor. It release the
  // references taken previously and release the objects depending on them.
  void Initialize(Browser* browser, TabStripUIEmbedder* embedder);
  void Deinitialize();

  // The embedder should call this whenever the result of
  // Embedder::GetLayout() changes.
  void LayoutChanged();

  // The embedder should call this whenever the tab strip gains keyboard focus.
  void ReceivedKeyboardFocus();

 private:
  // tab_strip::mojom::PageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<tab_strip::mojom::Page> page,
      mojo::PendingReceiver<tab_strip::mojom::PageHandler> receiver) override;

  WebuiLoadTimer webui_load_timer_;

  std::unique_ptr<TabStripPageHandler> page_handler_;

  std::unique_ptr<ui::ColorChangeHandler> color_provider_handler_;

  mojo::Receiver<tab_strip::mojom::PageHandlerFactory> page_factory_receiver_{
      this};

  raw_ptr<Browser> browser_ = nullptr;

  raw_ptr<TabStripUIEmbedder> embedder_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_TAB_STRIP_TAB_STRIP_UI_H_
