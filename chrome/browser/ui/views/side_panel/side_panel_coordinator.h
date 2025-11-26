// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_pinning_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"
#include "ui/views/view_observer.h"

class BrowserView;
class SidePanel;

namespace views {
class View;
}  // namespace views

// Class used to manage the state of side-panel content. Clients should manage
// side-panel visibility using this class rather than explicitly showing/hiding
// the side-panel View.
// This class is also responsible for consolidating multiple SidePanelEntry
// classes across multiple SidePanelRegistry instances, potentially merging them
// into a single unified side panel.
// Existence and value of registries' active_entry() determines which entry is
// visible for a given tab where the order of precedence is contextual
// registry's active_entry() then global registry's.
class SidePanelCoordinator final : public SidePanelUIBase,
                                   public views::ViewObserver {
 public:
  explicit SidePanelCoordinator(BrowserView* browser_view);
  SidePanelCoordinator(const SidePanelCoordinator&) = delete;
  SidePanelCoordinator& operator=(const SidePanelCoordinator&) = delete;
  ~SidePanelCoordinator() override;

  void Init(Browser* browser);
  void TearDownPreBrowserWindowDestruction();

  // SidePanelUI:
  using SidePanelUI::Close;
  void Close(SidePanelEntry::PanelType panel_type,
             SidePanelEntryHideReason reason,
             bool suppress_animations) override;
  void Toggle(SidePanelEntryKey key,
              SidePanelUtil::SidePanelOpenTrigger open_trigger) override;
  void ShowFrom(SidePanelEntryKey entry_key,
                gfx::Rect starting_bounds_in_browser_coordinates) override;

  // SidePanelUIBase:
  using SidePanelUIBase::Show;
  void Show(const UniqueKey& entry,
            std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
            bool suppress_animations) override;

  void SetNoDelaysForTesting(bool no_delays_for_testing) override;

  content::WebContents* GetWebContentsForTest(SidePanelEntryId id) override;
  void DisableAnimationsForTesting() override;

  SidePanelEntry* GetLoadingEntryForTesting(
      SidePanelEntry::PanelType type) const;

 private:
  // Returns the corresponding entry for `entry_key` or a nullptr if this key is
  // not registered in the currently observed registries. This looks through the
  // active contextual registry first, then the global registry.
  SidePanelEntry* GetEntryForKey(const SidePanelEntry::Key& entry_key) const;

  // SidePanelUIBase:
  void PopulateSidePanel(
      bool suppress_animations,
      const UniqueKey& unique_key,
      std::optional<SidePanelUtil::SidePanelOpenTrigger> open_trigger,
      SidePanelEntry* entry,
      std::optional<std::unique_ptr<views::View>> content_view) override;
  void MaybeShowEntryOnTabStripModelChanged(
      SidePanelRegistry* old_contextual_registry,
      SidePanelRegistry* new_contextual_registry) override;

  // Clear cached views with the corresponding panel type for registry entries
  // for global and contextual registries.
  void ClearCachedEntryViews(SidePanelEntry::PanelType type);

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_from,
                               bool visible) override;

  // Closes `promo_feature` if showing and if actual_id == promo_id, also
  // notifies the User Education system that the feature was used.
  void ClosePromoAndMaybeNotifyUsed(const base::Feature& promo_feature,
                                    SidePanelEntryId promo_id,
                                    SidePanelEntryId actual_id);

  // Returns the corresponding side panel for the provided panel type.
  SidePanel* GetSidePanelFor(SidePanelEntry::PanelType type);

  const raw_ptr<BrowserView, AcrossTasksDanglingUntriaged> browser_view_;

  std::unique_ptr<SidePanelToolbarPinningController>
      side_panel_toolbar_pinning_controller_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_COORDINATOR_H_
