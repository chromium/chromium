// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_EXTENSION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_EXTENSION_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/common/extension_id.h"

class OrganizerPanelController;

namespace actions {
class ActionItem;
}

namespace extensions {
class ExtensionHost;
class ExtensionViewHost;
}  // namespace extensions

// Implements an organizer panel that hosts a single extension rather than the
// normal Organizer UI. Enabled by turning on the feature
// "ShowExtensionsSidePanelUiInOrganizerPanel" in builds that support
// extensions.
class OrganizerPanelExtensionView : public OrganizerPanelView,
                                    public ExtensionViewViews::Observer,
                                    public extensions::ExtensionHostObserver {
 public:
  explicit OrganizerPanelExtensionView(BrowserWindowInterface& browser);
  ~OrganizerPanelExtensionView() override;

  // OrganizerPanelView:
  bool IsInExtensionModeForTesting() const override;

 private:
  void ResetExtensionContent();
  void HandleCloseExtensionHost(extensions::ExtensionHost* host);
  void UpdateExtensionContent(const extensions::ExtensionId& extension_id);
  void UpdateDefaultExtensionContent();

  // ExtensionViewViews::Observer:
  void OnViewDestroying() override;

  // extensions::ExtensionHostObserver:
  void OnExtensionHostDestroyed(extensions::ExtensionHost* host) override;

  void OnOrganizerPanelStateChanged(OrganizerPanelController* state_controller);

 private:
  const raw_ref<BrowserWindowInterface> browser_;
  const raw_ptr<actions::ActionItem> root_action_item_;
  const base::CallbackListSubscription state_controller_subscription_;
  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
  base::ScopedObservation<extensions::ExtensionHost,
                          extensions::ExtensionHostObserver>
      scoped_host_observation_{this};
  std::unique_ptr<extensions::ExtensionViewHost> extension_host_;
  raw_ptr<ExtensionViewViews> extension_view_ = nullptr;
  std::optional<extensions::ExtensionId> current_extension_id_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_EXTENSION_VIEW_H_
