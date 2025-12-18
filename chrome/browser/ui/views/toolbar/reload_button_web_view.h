// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/views/toolbar/reload_control.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/menus/simple_menu_model.h"
#include "ui/views/view.h"

class ReloadButtonUI;
class BrowserWindowInterface;

namespace views {
class MenuRunner;
class WebView;
}  // namespace views

// A view that displays the reload button as a WebView.
class ReloadButtonWebView : public views::View,
                            public ReloadControl,
                            public content::WebContentsDelegate,
                            public content::WebContentsObserver {
  METADATA_HEADER(ReloadButtonWebView, views::View)

 public:
  ReloadButtonWebView(BrowserWindowInterface* browser,
                      chrome::BrowserCommandController* controller);
  ReloadButtonWebView(const ReloadButtonWebView&) = delete;
  ReloadButtonWebView& operator=(const ReloadButtonWebView&) = delete;
  ~ReloadButtonWebView() override;

  // ReloadControl overrides:
  void ChangeMode(ReloadControl::Mode mode, bool force) override;
  bool GetMenuEnabled() const override;
  void SetMenuEnabled(bool is_menu_enabled) override;
  views::View* GetAsViewClassForTesting() override;

  // views::View:
  void AddedToWidget() override;

  // content::WebContentsDelegate:
  bool HandleContextMenu(content::RenderFrameHost& render_frame_host,
                         const content::ContextMenuParams& params) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool IsCommandIdVisible(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // content::WebContentsObserver:
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  void UpdateAccessibleHasPopup();
  void SetReloadButtonUIState();
  void UpdateTooltipText();

  raw_ptr<ReloadButtonUI> reload_button_ui_ = nullptr;
  raw_ptr<views::WebView> web_view_ = nullptr;
  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<chrome::BrowserCommandController> controller_;
  std::unique_ptr<ui::SimpleMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  bool is_menu_enabled_ = false;
  ReloadControl::Mode mode_ = ReloadControl::Mode::kReload;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_RELOAD_BUTTON_WEB_VIEW_H_
