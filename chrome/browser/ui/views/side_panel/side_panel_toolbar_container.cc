// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"

#include <memory>

#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

namespace {

class PinnedSidePanelToolbarButton : public ToolbarButton {
 public:
  PinnedSidePanelToolbarButton(BrowserView* browser_view,
                               SidePanelEntry::Id id,
                               std::u16string name,
                               const gfx::VectorIcon& icon)
      : ToolbarButton(
            base::BindRepeating(&PinnedSidePanelToolbarButton::ButtonPressed,
                                base::Unretained(this))),
        browser_view_(browser_view),
        id_(id) {
    SetTooltipText(name);
    SetVectorIcon(icon);

    button_controller()->set_notify_action(
        views::ButtonController::NotifyAction::kOnPress);

    // Do not flip the icon for RTL languages.
    SetFlipCanvasOnPaintForRTLUI(false);
  }

  ~PinnedSidePanelToolbarButton() override = default;

  void ButtonPressed() {
    browser_view_->side_panel_coordinator()->Show(
        id_, SidePanelUtil::SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  }

 private:
  BrowserView* browser_view_;
  SidePanelEntry::Id id_;
};

}  // namespace

SidePanelToolbarContainer::SidePanelToolbarContainer(BrowserView* browser_view)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_view_(browser_view),
      side_panel_button_(new SidePanelToolbarButton(browser_view->browser())) {
  // So we only get enter/exit messages when the mouse enters/exits the whole
  // container, even if it is entering/exiting a specific toolbar pinned entry
  // button view, too.
  SetNotifyEnterExitOnChild(true);

  const views::FlexSpecification hide_icon_flex_specification =
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithWeight(0);
  GetTargetLayoutManager()
      ->SetFlexAllocationOrder(views::FlexAllocationOrder::kReverse)
      .SetDefault(views::kFlexBehaviorKey,
                  hide_icon_flex_specification.WithOrder(3));
  side_panel_button_->SetProperty(views::kFlexBehaviorKey,
                                  views::FlexSpecification());
  AddMainItem(side_panel_button_);
  CreatePinnedEntryButtons();
}

SidePanelToolbarContainer::~SidePanelToolbarContainer() {}

void SidePanelToolbarContainer::UpdateAllIcons() {
  GetSidePanelButton()->UpdateIcon();

  for (auto* const pinned_entry_button : pinned_entry_buttons_) {
    pinned_entry_button->UpdateIcon();
  }
}

SidePanelToolbarButton* SidePanelToolbarContainer::GetSidePanelButton() const {
  return side_panel_button_.get();
}

void SidePanelToolbarContainer::CreatePinnedEntryButtons() {
  DCHECK(pinned_entry_buttons_.empty());

  // The only pinned entry is the search companion. Add it here directly.
  // If we support pinning side panel entries more broadly using this container
  // then we can fetch the name and icon from the entry itself and update pinned
  // entry toolbar buttons as the coordinator becomes aware of them. This sort
  // of observation is unnecessary for now when there is only one pinned entry.
  auto* search_companion_coordinator =
      SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(
          browser_view_->browser());
  auto button = std::make_unique<PinnedSidePanelToolbarButton>(
      browser_view_, SidePanelEntry::Id::kSearchCompanion,
      search_companion_coordinator->name(),
      search_companion_coordinator->icon());
  pinned_entry_buttons_.push_back(AddChildView(std::move(button)));

  ReorderViews();
}

void SidePanelToolbarContainer::ReorderViews() {
  // The main button is always last.
  ReorderChildView(main_item(), children().size());
}
