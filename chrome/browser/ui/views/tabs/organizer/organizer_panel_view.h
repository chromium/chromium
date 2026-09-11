// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"
#include "ui/views/view_tracker.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_id.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionViewViews;
namespace extensions {
class ExtensionHost;
class ExtensionViewHost;
}  // namespace extensions
#endif

class BrowserWindowInterface;
class OrganizerPanelStateController;

// Parent view of the Organizer Panel - holds together the views
// hierarchy including controls and close action.
class OrganizerPanelView : public views::View {
  METADATA_HEADER(OrganizerPanelView, views::View)

 public:
  OrganizerPanelView(BrowserWindowInterface* browser,
                     actions::ActionItem* root_action_item,
                     OrganizerPanelStateController* state_controller);
  OrganizerPanelView(const OrganizerPanelView&) = delete;
  OrganizerPanelView& operator=(const OrganizerPanelView&) = delete;
  ~OrganizerPanelView() override;

  // Called when the organizer panel state changes. Updates the visibility and
  // tooltips to match the new state.
  void OnOrganizerPanelStateChanged(
      OrganizerPanelStateController* state_controller);

  views::View* GetWebViewForTesting();
#if BUILDFLAG(ENABLE_EXTENSIONS)
  bool has_extension_observer_helper_for_testing() const {
    return extension_observer_helper_ != nullptr;
  }
#endif

  // views::View:
  void Layout(PassKey) override;

 private:
  void ClosePanel();

  const raw_ptr<BrowserWindowInterface> browser_;
  const raw_ptr<actions::ActionItem> root_action_item_;
  const base::CallbackListSubscription state_controller_subscription_;
  const base::CallbackListSubscription animation_subscription_;

  raw_ptr<views::View> web_view_ = nullptr;

  // Records the last time the panel was opened. Used for recording how long the
  // panel was open.
  base::TimeTicks last_opened_time_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  class ExtensionObserverHelper;

  void OnViewDestroying();
  void OnExtensionHostDestroyed(extensions::ExtensionHost* host);

  void HandleCloseExtensionHost(extensions::ExtensionHost* host);
  void UpdateExtensionContent(const extensions::ExtensionId& extension_id);
  void UpdateDefaultExtensionContent();
  void ResetExtensionContent();

  std::unique_ptr<extensions::ExtensionViewHost> extension_host_;
  raw_ptr<ExtensionViewViews> extension_view_ = nullptr;
  std::optional<extensions::ExtensionId> current_extension_id_;
  std::unique_ptr<ExtensionObserverHelper> extension_observer_helper_;
#endif

  base::WeakPtrFactory<OrganizerPanelView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ORGANIZER_ORGANIZER_PANEL_VIEW_H_
