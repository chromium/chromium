// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_side_panel_coordinator.h"

#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/extensions/side_panel/internal/extension_side_panel_contents_android.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"

namespace extensions {

namespace {

class ExtensionSidePanelCoordinatorAndroid
    : public ExtensionSidePanelCoordinator::Delegate,
      public ExtensionSidePanelContentsAndroid::Observer {
 public:
  explicit ExtensionSidePanelCoordinatorAndroid(
      ExtensionSidePanelCoordinator* coordinator)
      : coordinator_(coordinator) {}
  ~ExtensionSidePanelCoordinatorAndroid() override = default;

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
      return nullptr;
    }

    ui::WindowAndroid* window_android = nullptr;
    if (coordinator_->tab_interface() &&
        coordinator_->tab_interface()->GetContents() &&
        coordinator_->tab_interface()->GetContents()->GetNativeView()) {
      window_android = coordinator_->tab_interface()
                           ->GetContents()
                           ->GetNativeView()
                           ->GetWindowAndroid();
    }
    if (!window_android) {
      BrowserWindowInterface* browser = coordinator_->GetBrowser();
      if (browser && browser->GetWindow()) {
        window_android = browser->GetWindow()->GetNativeWindow();
      }
    }
    if (!window_android) {
      return nullptr;
    }

    auto contents = std::make_unique<ExtensionSidePanelContentsAndroid>(
        host, window_android);
    if (!contents->view()) {
      return nullptr;
    }
    scoped_view_observation_.Reset();
    scoped_view_observation_.Observe(contents.get());
    return contents;
  }

  void CloseSidePanel(SidePanelEntry* entry) override {
    BrowserWindowInterface* browser = coordinator_->GetBrowser();
    DCHECK(browser);
    auto* const side_panel_ui = SidePanelUI::From(browser);
    DCHECK(entry);
    const bool for_tab = coordinator_->tab_interface() != nullptr;
    if (side_panel_ui &&
        side_panel_ui->IsSidePanelEntryShowing(entry->key(), for_tab)) {
      side_panel_ui->Close(SidePanelEntryHideReason::kSidePanelClosed,
                           /*suppress_animations=*/false);
    } else {
      entry->ClearCachedView();
    }
  }

  // ExtensionSidePanelContentsAndroid::Observer:
  void OnViewDestroying() override {
    scoped_view_observation_.Reset();
    coordinator_->OnViewDestroyed();
  }

 private:
  raw_ptr<ExtensionSidePanelCoordinator> coordinator_;
  base::ScopedObservation<ExtensionSidePanelContentsAndroid,
                          ExtensionSidePanelContentsAndroid::Observer>
      scoped_view_observation_{this};
};

}  // namespace

// static
std::unique_ptr<ExtensionSidePanelCoordinator::Delegate>
ExtensionSidePanelCoordinator::CreateDelegate(
    ExtensionSidePanelCoordinator* coordinator) {
  return std::make_unique<ExtensionSidePanelCoordinatorAndroid>(coordinator);
}

}  // namespace extensions
