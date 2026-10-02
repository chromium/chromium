// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter.h"
#include "content/public/browser/web_contents_observer.h"

class CommandUpdater;
class BrowserWindowInterface;

namespace browser_controls_api {

// Adapter implementation for the desktop platform.
class BrowserControlsAdapterImpl : public BrowserControlsAdapter,
                                   public content::WebContentsObserver {
 public:
  BrowserControlsAdapterImpl(BrowserWindowInterface* browser_interface,
                             CommandUpdater* command_updater,
                             content::WebContents* web_contents);
  BrowserControlsAdapterImpl(const BrowserControlsAdapterImpl&&) = delete;
  BrowserControlsAdapterImpl operator=(const BrowserControlsAdapterImpl&&) =
      delete;
  ~BrowserControlsAdapterImpl() override;

  // BrowserControlsAdapter:
  void Reload(bool bypass_cache, WindowOpenDisposition disposition) override;
  void Stop() override;
  void Back(WindowOpenDisposition disposition) override;
  void Forward(WindowOpenDisposition disposition) override;
  void BackButtonHovered() override;
  void CreateNewSplitTab() override;
  void NavigateHome(WindowOpenDisposition disposition) override;
  void Navigate(const GURL& url) override;
  void NavigateText(const std::string& text) override;
  webui_toolbar::TabSplitStatus ComputeSplitTabStatus() override;

 private:
  // Helper to retrieve and reset the drag origin state.
  bool GetDragOriginatedFromRendererAndReset();

  // Helper to retrieve and reset whether the unfiltered drag URL had a
  // `javascript:` scheme.
  bool GetDragHasJavaScriptUrlAndReset();

  // Opens `url` in the current tab on behalf of a drop. Navigations from
  // renderer-originated drags get an opaque initiator origin.
  void OpenDroppedUrl(const GURL& url, bool drag_originated_from_renderer);

  // Not owned.
  const base::raw_ref<BrowserWindowInterface> browser_;
  const base::raw_ref<CommandUpdater> command_updater_;
};

}  // namespace browser_controls_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_ADAPTERS_BROWSER_CONTROLS_ADAPTER_IMPL_H_
