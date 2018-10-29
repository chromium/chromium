// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "url/gurl.h"

class ExtensionViewViews;

namespace views {
class Widget;
}

namespace content {
class DevToolsAgentHost;
}

namespace extensions {
class ExtensionViewHost;
}

// The bubble used for hosting a browser-action popup provided by an extension.
class ExtensionPopup : public views::BubbleDialogDelegateView,
                       public ExtensionViewViews::Container,
                       public content::NotificationObserver,
                       public TabStripModelObserver,
                       public content::DevToolsAgentHostObserver {
 public:
  enum ShowAction {
    SHOW,
    SHOW_AND_INSPECT,
  };

  ~ExtensionPopup() override;

  // Create and show a popup with the given |host| positioned adjacent to
  // |anchor_view|.
  // The positioning of the pop-up is determined by |arrow| according to the
  // following logic: The popup is anchored so that the corner indicated by the
  // value of |arrow| remains fixed during popup resizes.  If |arrow| is
  // BOTTOM_*, then the popup 'pops up', otherwise the popup 'drops down'.
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* ShowPopup(
      std::unique_ptr<extensions::ExtensionViewHost> host,
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      ShowAction show_action);

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // views::BubbleDialogDelegateView overrides.
  int GetDialogButtons() const override;

  // content::NotificationObserver overrides.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // ExtensionViewViews::Container overrides.
  void OnExtensionSizeChanged(ExtensionViewViews* view) override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;
  void AddedToWidget() override;

  // views::WidgetObserver overrides.
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // TabStripModelObserver overrides.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // The min/max height of popups.
  static const int kMinWidth;
  static const int kMinHeight;
  static const int kMaxWidth;
  static const int kMaxHeight;

 protected:
  ExtensionPopup(extensions::ExtensionViewHost* host,
                 views::View* anchor_view,
                 views::BubbleBorder::Arrow arrow,
                 ShowAction show_action);

  void CloseUnlessUnderInspection();

 private:
  static ExtensionPopup* Create(extensions::ExtensionViewHost* host,
                                views::View* anchor_view,
                                views::BubbleBorder::Arrow arrow,
                                ShowAction show_action);

  // Show the bubble, focus on its content, and register listeners.
  void ShowBubble();

  // content::DevToolsAgentHostObserver overrides.
  void DevToolsAgentHostAttached(
      content::DevToolsAgentHost* agent_host) override;
  void DevToolsAgentHostDetached(
      content::DevToolsAgentHost* agent_host) override;

  ExtensionViewViews* GetExtensionView();

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  // Flag used to indicate if the pop-up should open a devtools window once
  // it is shown inspecting it.
  bool inspect_with_devtools_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
