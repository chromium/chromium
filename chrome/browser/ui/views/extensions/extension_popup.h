// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "content/public/browser/devtools_agent_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"
#include "url/gurl.h"

class ExtensionViewViews;

namespace content {
class BrowserContext;
class DevToolsAgentHost;
}

namespace extensions {
class Extension;
class ExtensionViewHost;
enum class UnloadedExtensionReason;
}

// The bubble used for hosting a browser-action popup provided by an extension.
class ExtensionPopup : public views::BubbleDialogDelegateView,
                       public views::WidgetObserver,
                       public ExtensionViewViews::Container,
                       public extensions::ExtensionRegistryObserver,
                       public content::WebContentsObserver,
                       public TabStripModelObserver,
                       public content::DevToolsAgentHostObserver {
  METADATA_HEADER(ExtensionPopup, views::BubbleDialogDelegateView)

 public:
  // The min/max height of popups.
  // The minimum is just a little larger than the size of the button itself.
  // The maximum is an arbitrary number and should be smaller than most screens.
  static constexpr gfx::Size kMinSize = {25, 25};
  static constexpr gfx::Size kMaxSize = {800, 600};

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
                        PopupShowAction show_action,
                        ShowPopupCallback callback);

  ExtensionPopup(const ExtensionPopup&) = delete;
  ExtensionPopup& operator=(const ExtensionPopup&) = delete;
  ~ExtensionPopup() override;

  extensions::ExtensionViewHost* host() const { return host_.get(); }

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void AddedToWidget() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetTreeActivated(views::Widget* root_widget,
                             views::Widget* active_widget) override;

  // ExtensionViewViews::Container:
  gfx::Size GetMinBounds() override;
  gfx::Size GetMaxBounds() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

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

  // Closes the popup immediately, if possible. On Mac, if a nested
  // run loop is running, schedule a deferred close for after the
  // nested loop ends.
  void CloseDeferredIfNecessary(views::Widget::ClosedReason reason =
                                    views::Widget::ClosedReason::kUnspecified);

  // Returns the most recently constructed popup. For testing only.
  static ExtensionPopup* last_popup_for_testing();

 private:
  class ScopedDevToolsAgentHostObservation;

  ExtensionPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                 views::View* anchor_view,
                 views::BubbleBorder::Arrow arrow,
                 PopupShowAction show_action,
                 ShowPopupCallback callback);

  // Shows the bubble, focuses its content, and registers listeners.
  void ShowBubble();

  // Closes the bubble unless if there is
  //   1. an attached DevTools inspection window, or
  //   2. an open web dialog, e.g. JS alert.
  void CloseUnlessBlockedByInspectionOrJSDialog();

  // Handles a signal from the extension host to close.
  void HandleCloseExtensionHost(extensions::ExtensionHost* host);

  // The contained host for the view.
  std::unique_ptr<extensions::ExtensionViewHost> host_;

  raw_ptr<ExtensionViewViews, DanglingUntriaged> extension_view_;

  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  PopupShowAction show_action_;

  ShowPopupCallback shown_callback_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      anchor_widget_observation_{this};

  // Note: This must be reset *before* `host_`. See note in
  // OnExtensionUnloaded().
  std::unique_ptr<ScopedDevToolsAgentHostObservation>
      scoped_devtools_observation_;

  base::WeakPtrFactory<ExtensionPopup> deferred_close_weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
