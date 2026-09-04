// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_side_panel_coordinator.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/side_panel/extensions/extension_side_panel_action_item_util.h"
#include "extensions/browser/extension_icon_image.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/actions/actions.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace extensions {

namespace {

class ExtensionSidePanelCoordinatorViews
    : public ExtensionSidePanelCoordinator::Delegate,
      public ExtensionViewViews::Observer {
 public:
  explicit ExtensionSidePanelCoordinatorViews(
      ExtensionSidePanelCoordinator* coordinator)
      : coordinator_(coordinator) {}
  ~ExtensionSidePanelCoordinatorViews() override = default;

  // ExtensionSidePanelCoordinator::Delegate:
  std::unique_ptr<ExtensionViewHost> CreateHost(const GURL& url) override {
    return ExtensionViewHostFactory::CreateSidePanelHost(
        *coordinator_->extension(), url, coordinator_->browser(),
        coordinator_->tab_interface());
  }

  SidePanelNativeView CreateView(SidePanelEntryScope& scope) override {
    ExtensionViewHost* host = coordinator_->host();
    if (!host) {
      DCHECK(!ExtensionRegistry::Get(coordinator_->profile())
                  ->enabled_extensions()
                  .GetByID(coordinator_->extension()->id()));
      return std::make_unique<views::WebView>(/*browser_context=*/nullptr);
    }

    auto extension_view =
        std::make_unique<ExtensionViewViews>(coordinator_->profile(), host);
    extension_view->SetVisible(true);
    scoped_view_observation_.Reset();
    scoped_view_observation_.Observe(extension_view.get());
    return extension_view;
  }

  void CloseSidePanel(SidePanelEntry* entry) override {
    CHECK(entry);
    BrowserWindowInterface* browser = coordinator_->GetBrowser();
    CHECK(browser);
    auto* const side_panel_ui = SidePanelUI::From(browser);
    const bool for_tab = coordinator_->tab_interface() != nullptr;
    if (side_panel_ui &&
        side_panel_ui->IsSidePanelEntryShowing(entry->key(), for_tab)) {
      side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                           /*suppress_animations=*/false);
    } else {
      entry->ClearCachedView();
    }
  }

  void OnEntryRegistered() override { AcquireActionItemReference(); }

  void OnEntryDeregistered() override { ReleaseActionItemReference(); }

  void OnIconUpdated() override { UpdateActionItemIcon(); }

  // Called when a tab-scoped coordinator's tab is about to leave its window.
  // Releases the reference held on the current window's shared action item.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason) override {
    switch (reason) {
      case tabs::TabInterface::DetachReason::kDelete:
        // The tab and its entry are being destroyed. Deregister the entry
        // before its reference is released so the shared action item is never
        // removed while a registered entry still references it.
        coordinator_->DeregisterEntry();
        break;
      case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
        // The entry moves with the tab. Release the reference on the window the
        // tab is leaving; DidInsert reacquires it on the destination window.
        ReleaseActionItemReference();
        break;
    }
  }

  // Called when a tab-scoped coordinator's tab is inserted into a window.
  // Reacquires a reference on the (possibly new) window's shared action item.
  void OnTabDidInsert(tabs::TabInterface* tab) override {
    // Reacquire on the window the tab now belongs to. No entry is registered
    // yet on the tab's initial insertion, so this is a no-op then.
    if (coordinator_->GetEntry()) {
      AcquireActionItemReference();
    }
  }

  // ExtensionViewViews::Observer:
  void OnViewDestroying() override {
    scoped_view_observation_.Reset();
    coordinator_->OnViewDestroyed();
  }

 private:
  // Acquires or releases this coordinator's single reference to the extension's
  // shared action item, no-op if the reference is already held / not held. The
  // reference keeps the action item alive while this coordinator's entry is
  // registered.
  void AcquireActionItemReference() {
    if (holds_action_item_reference_) {
      return;
    }
    // A tab that is between windows has no browser; DidInsert acquires the
    // reference once the tab is inserted into a window.
    BrowserWindowInterface* browser = coordinator_->GetBrowser();
    if (!browser) {
      return;
    }
    side_panel_action_item_util::AcquireActionItem(browser,
                                                   *coordinator_->extension());
    holds_action_item_reference_ = true;
    UpdateActionItemIcon();
  }

  void ReleaseActionItemReference() {
    if (!holds_action_item_reference_) {
      return;
    }
    BrowserWindowInterface* browser = coordinator_->GetBrowser();
    if (browser) {
      side_panel_action_item_util::ReleaseActionItem(
          browser, coordinator_->extension()->id());
    }
    holds_action_item_reference_ = false;
  }

  // Adds the icon of the extension to the action item. This action item is
  // later retrieved by the side panel coordinator to show the side panel.
  void UpdateActionItemIcon() {
    if (!holds_action_item_reference_) {
      return;
    }
    std::optional<actions::ActionId> extension_action_id =
        actions::ActionIdMap::StringToActionId(
            coordinator_->GetEntryKey().ToString());
    if (!extension_action_id.has_value()) {
      return;
    }
    BrowserWindowInterface* browser = coordinator_->GetBrowser();
    if (!browser) {
      return;
    }
    BrowserActions* browser_actions = BrowserActions::From(browser);
    if (!browser_actions) {
      return;
    }
    actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
        extension_action_id.value(), browser_actions->root_action_item());
    if (action_item && coordinator_->extension_icon()) {
      action_item->SetImage(
          ui::ImageModel::FromImage(coordinator_->extension_icon()->image()));
    }
  }

  raw_ptr<ExtensionSidePanelCoordinator> coordinator_;

  // Whether this coordinator currently holds a reference to the extension's
  // shared action item. This usually mirrors whether its SidePanelEntry is
  // registered, but the two diverge while the tab is detached between windows:
  // the entry stays registered while the reference is dropped (no window owns
  // the action item) and is reacquired on the destination window in DidInsert.
  bool holds_action_item_reference_ = false;

  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
};

}  // namespace

// static
std::unique_ptr<ExtensionSidePanelCoordinator::Delegate>
ExtensionSidePanelCoordinator::CreateDelegate(
    ExtensionSidePanelCoordinator* coordinator) {
  return std::make_unique<ExtensionSidePanelCoordinatorViews>(coordinator);
}

}  // namespace extensions
