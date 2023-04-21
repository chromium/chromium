// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"
#include "components/prefs/pref_change_registrar.h"

class BrowserView;
class SidePanelToolbarButton;
class ToolbarButton;

// Container for side panel button and pinned side panel entries shown in the
// toolbar.
class SidePanelToolbarContainer : public ToolbarIconContainerView {
 public:
  explicit SidePanelToolbarContainer(BrowserView* browser_view);
  SidePanelToolbarContainer(const SidePanelToolbarContainer&) = delete;
  SidePanelToolbarContainer& operator=(const SidePanelToolbarContainer&) =
      delete;
  ~SidePanelToolbarContainer() override;

  // Gets the side panel button for the toolbar.
  SidePanelToolbarButton* GetSidePanelButton() const;

  void ObserveSidePanelView(views::View* side_panel);

  // Creates any pinned side panel entry toolbar buttons.
  void CreatePinnedEntryButtons();

  void AddPinnedEntryButtonFor(SidePanelEntry::Id id,
                               std::u16string name,
                               const gfx::VectorIcon& icon);
  void RemovePinnedEntryButtonFor(SidePanelEntry::Id id);

  // Returns true if a button exists and is pinned for the given Id. Note, this
  // is not an idication of whether the button is currently visible to users, on
  // small windows the button could be hidden though technically still "pinned".
  bool IsPinned(SidePanelEntry::Id id);

  void UpdateSidePanelContainerButtonsState();

  bool IsActiveEntryPinnedAndVisible();

  // ToolbarIconContainerView:
  void UpdateAllIcons() override;

 private:
  class PinnedSidePanelToolbarButton : public ToolbarButton {
   public:
    PinnedSidePanelToolbarButton(BrowserView* browser_view,
                                 SidePanelEntry::Id id,
                                 std::u16string name,
                                 const gfx::VectorIcon& icon);
    ~PinnedSidePanelToolbarButton() override;

    SidePanelEntry::Id id() { return id_; }

    void ButtonPressed();
    void UnpinForContextMenu(int event_flags);

   private:
    std::unique_ptr<ui::MenuModel> CreateMenuModel();

    raw_ptr<BrowserView> browser_view_;
    SidePanelEntry::Id id_;
  };

  // Indicates whether the button exists, this does not necessarily mean it is
  // pinned at this time.
  bool HasPinnedEntryButtonFor(SidePanelEntry::Id id);

  // Sorts child views to display them in the correct order (pinned buttons,
  // side panel button).
  void ReorderViews();

  void OnPinnedButtonPrefChanged();

  void UpdatePinnedButtonsVisibility();

  const raw_ptr<BrowserView> browser_view_;

  const raw_ptr<SidePanelToolbarButton> side_panel_button_;

  std::vector<PinnedSidePanelToolbarButton*> pinned_entry_buttons_;
  base::CallbackListSubscription side_panel_visibility_change_subscription_;
  base::CallbackListSubscription pinned_button_visibility_change_subscription_;
  PrefChangeRegistrar pref_change_registrar_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_TOOLBAR_CONTAINER_H_
