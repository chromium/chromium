// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/commander/commander_frontend.h"
#include "chrome/browser/ui/webui/commander/commander_handler.h"
#include "ui/views/widget/widget_observer.h"

class CommanderFocusLossWatcher;
class CommanderWebView;
class Profile;

namespace views {
class WidgetDelegate;
class Widget;
}  // namespace views

namespace commander {
class CommanderBackend;
struct CommanderViewModel;
}  // namespace commander

// Views implementation of commander::CommanderFrontend. The actual UI is WebUI;
// this class is responsible for setting up the infrastructure to host the
// WebUI in its own widget and mediating between the WebUI implementation and
// the controller.
class CommanderFrontendViews : public commander::CommanderFrontend,
                               public CommanderHandler::Delegate,
                               public BrowserListObserver,
                               public views::WidgetObserver {
 public:
  explicit CommanderFrontendViews(commander::CommanderBackend* backend);
  ~CommanderFrontendViews() override;

  // commander::CommanderFrontend overrides
  void ToggleForBrowser(Browser* browser) override;
  void Show(Browser* browser) override;
  void Hide() override;

  // CommanderHandler::Delegate overrides
  void OnTextChanged(const std::u16string& text) override;
  void OnOptionSelected(size_t option_index, int result_set_id) override;
  void OnCompositeCommandCancelled() override;
  void OnDismiss() override;
  void OnHeightChanged(int new_height) override;
  void OnHandlerEnabled(bool is_enabled) override;

  // BrowserListObserver overrides
  void OnBrowserClosing(Browser* browser) override;

  // views::WidgetObserver overrides
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  // Receives view model updates from |backend_|.
  void OnViewModelUpdated(commander::CommanderViewModel view_model);

  // Creates a WebView to host the WebUI interface for |profile|.
  std::unique_ptr<CommanderWebView> CreateWebView(Profile* profile);

  bool is_showing() { return widget_ != nullptr; }

  void OnAppTerminating();

  // The controller. Must outlive this object.
  raw_ptr<commander::CommanderBackend> backend_;
  // The widget that contains |web_view_|
  raw_ptr<views::Widget> widget_ = nullptr;
  // |widget_|'s delegate
  std::unique_ptr<views::WidgetDelegate> widget_delegate_;
  // The WebView that hosts the WebUI instance. Populated when the view
  // is showing and null otherwise.
  raw_ptr<CommanderWebView> web_view_ = nullptr;
  // The browser |widget_| is attached to.
  raw_ptr<Browser> browser_ = nullptr;
  // Whether the web UI interface is loaded and ready to accept view models.
  bool is_handler_enabled_ = false;
  // Sbuscription for observing app termination.
  base::CallbackListSubscription subscription_;
  // Helper to close the commander widget on deactivation.
  std::unique_ptr<CommanderFocusLossWatcher> focus_loss_watcher_;

  base::WeakPtrFactory<CommanderFrontendViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_
