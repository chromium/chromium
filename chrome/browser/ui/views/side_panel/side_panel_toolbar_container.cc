// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_toolbar_container.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/user_metrics.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/search_companion/search_companion_side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/toolbar/side_panel_toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model_menu_model_adapter.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/layout/animating_layout_manager.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"

///////////////////////////////////////////////////////////////////////////////
// SidePanelToolbarContainer::PinnedSidePanelToolbarButton:
SidePanelToolbarContainer::PinnedSidePanelToolbarButton::
    PinnedSidePanelToolbarButton(BrowserView* browser_view,
                                 SidePanelEntry::Id id,
                                 std::u16string name,
                                 const gfx::VectorIcon& icon)
    : ToolbarButton(
          base::BindRepeating(&PinnedSidePanelToolbarButton::ButtonPressed,
                              base::Unretained(this)),
          CreateMenuModel(),
          nullptr),
      browser_view_(browser_view),
      id_(id) {
  SetTooltipText(name);
  SetVectorIcon(icon);

  button_controller()->set_notify_action(
      views::ButtonController::NotifyAction::kOnPress);

  // Do not flip the icon for RTL languages.
  SetFlipCanvasOnPaintForRTLUI(false);
}

SidePanelToolbarContainer::PinnedSidePanelToolbarButton::
    ~PinnedSidePanelToolbarButton() = default;

void SidePanelToolbarContainer::PinnedSidePanelToolbarButton::ButtonPressed() {
  auto* side_panel_ui =
      SidePanelUI::GetSidePanelUIForBrowser(browser_view_->browser());
  if (side_panel_ui->GetCurrentEntryId() ==
      SidePanelEntry::Id::kSearchCompanion) {
    side_panel_ui->Close();
  } else {
    side_panel_ui->Show(
        id_, SidePanelUtil::SidePanelOpenTrigger::kPinnedEntryToolbarButton);
  }
  // Close IPH for companion if shown and record usage for side panel promo.
  browser_view_->NotifyFeatureEngagementEvent(
      "companion_side_panel_accessed_via_toolbar_button");
  browser_view_->CloseFeaturePromo(
      feature_engagement::kIPHCompanionSidePanelFeature);
}

void SidePanelToolbarContainer::PinnedSidePanelToolbarButton::
    UnpinForContextMenu(int event_flags) {
  PrefService* pref_service = browser_view_->GetProfile()->GetPrefs();
  if (pref_service) {
    pref_service->SetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar,
                             false);
    base::RecordAction(base::UserMetricsAction(
        "SidePanel.Companion.Unpinned.ByPinnedButtonContextMenu"));
  }
}

std::unique_ptr<ui::MenuModel>
SidePanelToolbarContainer::PinnedSidePanelToolbarButton::CreateMenuModel() {
  ui::DialogModel::Builder dialog_model = ui::DialogModel::Builder();
  dialog_model.AddMenuItem(
      ui::ImageModel(),
      l10n_util::GetStringUTF16(IDS_SIDE_PANEL_TOOLBAR_BUTTON_CXMENU_UNPIN),
      base::BindRepeating(&PinnedSidePanelToolbarButton::UnpinForContextMenu,
                          base::Unretained(this)));
  return std::make_unique<ui::DialogModelMenuModelAdapter>(
      dialog_model.Build());
}

///////////////////////////////////////////////////////////////////////////////
// SidePanelToolbarContainer:

SidePanelToolbarContainer::SidePanelToolbarContainer(BrowserView* browser_view)
    : ToolbarIconContainerView(/*uses_highlight=*/true),
      browser_view_(browser_view),
      side_panel_button_(new SidePanelToolbarButton(browser_view->browser())) {
  pref_change_registrar_.Init(browser_view->GetProfile()->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kSidePanelCompanionEntryPinnedToToolbar,
      base::BindRepeating(&SidePanelToolbarContainer::OnPinnedButtonPrefChanged,
                          base::Unretained(this)));
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
  // Before creating the pinned buttons, verify that the pref value is correct
  // and update it if not. If the user has been moved into a different default
  // pin state group (i.e. from the default being false to the default being
  // true) we want to make sure their pin state changes if they have not
  // explicitly changed it themselves.
  if (PrefService* pref_service = browser_view_->GetProfile()->GetPrefs()) {
    companion::UpdateCompanionDefaultPinnedToToolbarState(pref_service);
  }
  CreatePinnedEntryButtons();
}

SidePanelToolbarContainer::~SidePanelToolbarContainer() {}

bool SidePanelToolbarContainer::IsActiveEntryPinnedAndVisible() {
  absl::optional<SidePanelEntry::Id> active_id =
      GetSidePanelCoordinator()->GetCurrentEntryId();
  for (auto* pinned_button : pinned_entry_buttons_) {
    if (pinned_button->id() == active_id) {
      return pinned_button->GetVisible();
    }
  }
  return false;
}

void SidePanelToolbarContainer::UpdateAllIcons() {
  GetSidePanelButton()->UpdateIcon();

  for (auto* const pinned_entry_button : pinned_entry_buttons_) {
    pinned_entry_button->UpdateIcon();
  }
}

SidePanelToolbarButton* SidePanelToolbarContainer::GetSidePanelButton() const {
  return side_panel_button_.get();
}

ToolbarButton& SidePanelToolbarContainer::GetPinnedButtonForId(
    SidePanelEntry::Id id) {
  const auto iter = base::ranges::find(
      pinned_entry_buttons_, id, [](auto* button) { return button->id(); });
  // TODO(crbug.com/1447841): Remove all companion related special case code
  // once a generalized path forward has been determined.
  CHECK(iter != pinned_entry_buttons_.end());
  return **iter;
}

void SidePanelToolbarContainer::ObserveSidePanelView(views::View* side_panel) {
  side_panel_visibility_change_subscription_ =
      side_panel->AddVisibleChangedCallback(base::BindRepeating(
          &SidePanelToolbarContainer::UpdateSidePanelContainerButtonsState,
          base::Unretained(this)));
}

void SidePanelToolbarContainer::CreatePinnedEntryButtons() {
  DCHECK(pinned_entry_buttons_.empty());

  // The only pinned entry is the search companion. Add it here directly if
  // supported. If we support pinning side panel entries more broadly using this
  // container then we can fetch the name and icon from the entry itself and
  // update pinned entry toolbar buttons as the coordinator becomes aware of
  // them. This sort of observation is unnecessary for now when there is only
  // one pinned entry.
  // Runtime availability checks are set to `true` because pinned buttons should
  // be created only if runtime checks pass.
  if (!SearchCompanionSidePanelCoordinator::IsSupported(
          browser_view_->GetProfile(), /*include_runtime_checks=*/true)) {
    return;
  }
  auto* search_companion_coordinator =
      SearchCompanionSidePanelCoordinator::GetOrCreateForBrowser(
          browser_view_->browser());
  AddPinnedEntryButtonFor(
      SidePanelEntry::Id::kSearchCompanion,
      search_companion_coordinator->GetTooltipForToolbarButton(),
      search_companion_coordinator->icon());
}

void SidePanelToolbarContainer::AddPinnedEntryButtonFor(
    SidePanelEntry::Id id,
    std::u16string name,
    const gfx::VectorIcon& icon) {
  if (HasPinnedEntryButtonFor(id)) {
    return;
  }
  auto button = std::make_unique<PinnedSidePanelToolbarButton>(browser_view_,
                                                               id, name, icon);
  button->SetProperty(views::kElementIdentifierKey,
                      kSidePanelCompanionToolbarButtonElementId);
  button->SetVisible(false);
  ObserveButton(button.get());
  pinned_button_visibility_change_subscription_ =
      button->AddVisibleChangedCallback(base::BindRepeating(
          &SidePanelToolbarContainer::UpdateSidePanelContainerButtonsState,
          base::Unretained(this)));
  pinned_entry_buttons_.push_back(AddChildView(std::move(button)));

  ReorderViews();
  UpdatePinnedButtonsVisibility();
}

void SidePanelToolbarContainer::RemovePinnedEntryButtonFor(
    SidePanelEntry::Id id) {
  if (!HasPinnedEntryButtonFor(id)) {
    return;
  }
  const auto iter = base::ranges::find(
      pinned_entry_buttons_, id, [](auto* button) { return button->id(); });
  DCHECK(iter != pinned_entry_buttons_.end());
  RemoveChildView(*iter);
  pinned_entry_buttons_.erase(iter);
  pinned_button_visibility_change_subscription_ =
      base::CallbackListSubscription();
}

bool SidePanelToolbarContainer::IsPinned(SidePanelEntry::Id id) {
  PrefService* pref_service = browser_view_->GetProfile()->GetPrefs();
  if (id == SidePanelEntry::Id::kSearchCompanion && pref_service) {
    return HasPinnedEntryButtonFor(id) &&
           pref_service->GetBoolean(
               prefs::kSidePanelCompanionEntryPinnedToToolbar);
  }
  return false;
}

void SidePanelToolbarContainer::UpdateSidePanelContainerButtonsState() {
  bool side_panel_visible = browser_view_->unified_side_panel()->GetVisible();
  bool side_panel_button_highlighted = side_panel_visible;
  absl::optional<SidePanelEntry::Id> current_active_id =
      GetSidePanelCoordinator()->GetCurrentEntryId();
  for (PinnedSidePanelToolbarButton* pinned_button : pinned_entry_buttons_) {
    if (browser_view_->unified_side_panel()->GetVisible() &&
        pinned_button->GetVisible() &&
        pinned_button->id() == current_active_id) {
      pinned_button->SetHighlighted(true);
      side_panel_button_highlighted = false;
    } else {
      pinned_button->SetHighlighted(false);
    }
  }
  GetSidePanelButton()->SetHighlighted(side_panel_button_highlighted);
}

bool SidePanelToolbarContainer::HasPinnedEntryButtonFor(SidePanelEntry::Id id) {
  const auto iter = base::ranges::find(
      pinned_entry_buttons_, id, [](auto* button) { return button->id(); });
  return iter != pinned_entry_buttons_.end();
}

void SidePanelToolbarContainer::ReorderViews() {
  // The main button is always last.
  ReorderChildView(main_item(), children().size());
}

void SidePanelToolbarContainer::OnPinnedButtonPrefChanged() {
  UpdatePinnedButtonsVisibility();
  GetSidePanelCoordinator()->UpdateHeaderPinButtonState();
}

void SidePanelToolbarContainer::UpdatePinnedButtonsVisibility() {
  if (pinned_entry_buttons_.empty()) {
    return;
  }
  PrefService* pref_service = browser_view_->GetProfile()->GetPrefs();
  if (pref_service) {
    bool should_be_pinned = pref_service->GetBoolean(
        prefs::kSidePanelCompanionEntryPinnedToToolbar);
    if (should_be_pinned && !pinned_entry_buttons_[0]->GetVisible()) {
      GetAnimatingLayoutManager()->FadeIn(pinned_entry_buttons_[0]);
    } else if (!should_be_pinned && pinned_entry_buttons_[0]->GetVisible()) {
      GetAnimatingLayoutManager()->FadeOut(pinned_entry_buttons_[0]);
    }
  }
}

SidePanelCoordinator* SidePanelToolbarContainer::GetSidePanelCoordinator() {
  return SidePanelUtil::GetSidePanelCoordinatorForBrowser(
      browser_view_->browser());
}
