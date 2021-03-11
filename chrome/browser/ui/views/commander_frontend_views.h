// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_

#include "chrome/browser/ui/commander/commander_frontend.h"

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/webui/commander/commander_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/widget/widget_observer.h"

class CommanderWebView;

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
                               public content::NotificationObserver,
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

  // content::NotificationObserver overrides
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // views::WidgetObserver overrides
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  // Receives view model updates from |backend_|.
  void OnViewModelUpdated(commander::CommanderViewModel view_model);
  // Receives system profile from CreateProfileAsync()
  void OnSystemProfileAvailable(Profile* profile, Profile::CreateStatus status);

  // Creates a web_view_ to host the WebUI interface for |profile|. Should only
  // be called once when the system profile becomes available.
  void CreateWebView(Profile* profile);

  bool is_showing() { return widget_ != nullptr; }
  bool is_web_view_created() {
    return is_showing() || web_view_.get() != nullptr;
  }

  // The controller. Must outlive this object.
  commander::CommanderBackend* backend_;
  // True if Show() was called before the system profile is available.
  // If this is set, CreateWebView() will call Show().
  bool show_requested_ = false;
  // The widget that contains |web_view_ptr_|
  views::Widget* widget_ = nullptr;
  // |widget_|'s delegate
  std::unique_ptr<views::WidgetDelegate> widget_delegate_;
  // The web_view_ that hosts the WebUI instance. Populated when the view
  // is showing and null otherwise.
  CommanderWebView* web_view_ptr_ = nullptr;
  // |web_view_ptr_| is held here when the widget is *not* showing.
  std::unique_ptr<CommanderWebView> web_view_;
  // The browser |widget_| is attached to.
  Browser* browser_ = nullptr;
  // Whether the web UI interface is loaded and ready to accept view models.
  bool is_handler_enabled_ = false;
  // Registrar for observing app termination.
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<CommanderFrontendViews> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_COMMANDER_FRONTEND_VIEWS_H_
