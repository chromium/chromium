// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/gurl.h"

#if defined(USE_AURA)
#include "ui/wm/public/activation_change_observer.h"
#endif

class ExtensionViewViews;

namespace content {
class DevToolsAgentHost;
}

namespace extensions {
class ExtensionViewHost;
}

// The bubble used for hosting a browser-action popup provided by an extension.
class ExtensionPopup : public views::BubbleDialogDelegateView,
#if defined(USE_AURA)
                       public wm::ActivationChangeObserver,
#endif
                       public ExtensionViewViews::Container,
                       public content::NotificationObserver,
                       public TabStripModelObserver,
                       public content::DevToolsAgentHostObserver {
 public:
  enum ShowAction {
    SHOW,
    SHOW_AND_INSPECT,
  };

  // The min/max height of popups.
  // The minimum is just a little larger than the size of the button itself.
  // The maximum is an arbitrary number and should be smaller than most screens.
  static constexpr int kMinWidth = 25;
  static constexpr int kMinHeight = 25;
  static constexpr int kMaxWidth = 800;
  static constexpr int kMaxHeight = 600;

  // Creates and shows a popup with the given |host| positioned adjacent to
  // |anchor_view|.
  // The positioning of the pop-up is determined by |arrow| according to the
  // following logic: The popup is anchored so that the corner indicated by the
  // value of |arrow| remains fixed during popup resizes.  If |arrow| is
  // BOTTOM_*, then the popup 'pops up', otherwise the popup 'drops down'.
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static void ShowPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                        views::View* anchor_view,
                        views::BubbleBorder::Arrow arrow,
                        ShowAction show_action);

  ~ExtensionPopup() override;

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
#if defined(USE_AURA)
  void OnWidgetDestroying(views::Widget* widget) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;
#endif

  // ExtensionViewViews::Container:
  void OnExtensionSizeChanged(ExtensionViewViews* view) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // content::DevToolsAgentHostObserver:
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

 private:
  ExtensionPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                 views::View* anchor_view,
                 views::BubbleBorder::Arrow arrow,
                 ShowAction show_action);

  // Shows the bubble, focuses its content, and registers listeners.
  void ShowBubble();

  // Closes the bubble if the devtools window is not attached.
  void CloseUnlessUnderInspection();

  ExtensionViewViews* GetExtensionView();

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  ShowAction show_action_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
