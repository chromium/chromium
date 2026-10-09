// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_segmented_control.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/color/color_id.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr auto kInsideBorderInsets = gfx::Insets(3);
constexpr int kBetweenChildSpacing = 4;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(VerticalTabStripSegmentedControl,
                                      kSegmentedControlElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(VerticalTabStripSegmentedControl,
                                      kTabSearchUnpinMenuItem);

VerticalTabStripSegmentedControl::VerticalTabStripSegmentedControl(
    BrowserWindowInterface* browser)
    : browser_(browser) {
  CHECK(browser_);
  SetProperty(views::kElementIdentifierKey, kSegmentedControlElementId);

  const int button_size = GetLayoutConstant(
      LayoutConstant::kVerticalTabStripTopContainerButtonSize);

  SetBackground(views::CreatePillBackground(ui::kColorSysSurface));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kInsideBorderInsets,
      kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  auto create_button = [&](Segment segment, int tooltip_id,
                           ui::ElementIdentifier element_id) {
    auto button = std::make_unique<views::ImageButton>(
        base::BindRepeating(&VerticalTabStripSegmentedControl::OnButtonPressed,
                            base::Unretained(this), segment));
    button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_id));
    button->SetProperty(views::kElementIdentifierKey, element_id);
    button->SetPreferredSize(gfx::Size(button_size, button_size));
    button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
    button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
    button->GetViewAccessibility().SetRole(ax::mojom::Role::kToggleButton);
    button->SetHasInkDropActionOnClick(true);
    views::InstallCircleHighlightPathGenerator(button.get());
    auto* const ink_drop = views::InkDrop::Get(button.get());
    ink_drop->SetMode(views::InkDropHost::InkDropMode::ON);
    ink_drop->SetLayerRegion(views::LayerRegion::kAbove);
    ink_drop->SetBaseColor(ui::kColorSysStateHoverOnSubtle);
    ink_drop->SetHighlightOpacity(1.0f);
    views::ImageButton* button_ptr = AddChildView(std::move(button));
    layout->SetFlexForView(button_ptr, 1);
    return button_ptr;
  };

  tab_strip_button_ =
      create_button(Segment::kTabStrip, IDS_TAB_STRIP_BUTTON_TOOLTIP,
                    kVerticalTabStripTabStripButtonElementId);
  organizer_button_ = create_button(Segment::kOrganizer, IDS_TOOLTIP_TAB_SEARCH,
                                    kTabSearchButtonElementId);
  organizer_button_->set_context_menu_controller(this);

  if (auto* controller = OrganizerPanelController::From(browser_)) {
    if (controller->IsOrganizerPanelVisible()) {
      active_segment_ = Segment::kOrganizer;
    }
    organizer_state_subscription_ = controller->RegisterOnStateChanged(
        base::IgnoreArgs<OrganizerPanelController*>(
            base::BindRepeating(&VerticalTabStripSegmentedControl::
                                    UpdateActiveSegmentFromController,
                                base::Unretained(this))));
  }

  UpdateButtonIcons();
  UpdateSegmentBackgrounds();
}

VerticalTabStripSegmentedControl::~VerticalTabStripSegmentedControl() = default;

void VerticalTabStripSegmentedControl::AddedToWidget() {
  paint_as_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &VerticalTabStripSegmentedControl::UpdateSegmentBackgrounds,
          base::Unretained(this)));
  UpdateSegmentBackgrounds();
}

void VerticalTabStripSegmentedControl::RemovedFromWidget() {
  paint_as_active_subscription_ = {};
}

views::ImageButton* VerticalTabStripSegmentedControl::GetButton(
    Segment segment) {
  switch (segment) {
    case Segment::kTabStrip:
      return tab_strip_button_;
    case Segment::kOrganizer:
      return organizer_button_;
  }
}

void VerticalTabStripSegmentedControl::OnButtonPressed(Segment segment) {
  if (auto* controller = OrganizerPanelController::From(browser_)) {
    switch (segment) {
      case Segment::kTabStrip:
        controller->SetOrganizerVisible(false);
        break;
      case Segment::kOrganizer:
        controller->SetOrganizerVisible(true);
        break;
    }
  }
}

void VerticalTabStripSegmentedControl::UpdateActiveSegmentFromController() {
  Segment new_segment = Segment::kTabStrip;
  if (auto* controller = OrganizerPanelController::From(browser_)) {
    if (controller->IsOrganizerPanelVisible()) {
      new_segment = Segment::kOrganizer;
    }
  }

  if (active_segment_ != new_segment) {
    active_segment_ = new_segment;
    if (tab_strip_button_) {
      tab_strip_button_->GetViewAccessibility().SetIsSelected(
          active_segment_ == Segment::kTabStrip);
    }
    if (organizer_button_) {
      organizer_button_->GetViewAccessibility().SetIsSelected(
          active_segment_ == Segment::kOrganizer);
    }
    UpdateButtonIcons();
    UpdateSegmentBackgrounds();
  }
}

void VerticalTabStripSegmentedControl::UpdateButtonIcons() {
  if (!tab_strip_button_ || !organizer_button_) {
    return;
  }

  const int icon_size =
      GetLayoutConstant(LayoutConstant::kVerticalTabStripComboButtonIconSize);

  auto update_button_icon = [&](Segment segment, const gfx::VectorIcon& icon) {
    const bool is_active = (active_segment_ == segment);
    const ui::ColorId color_id =
        is_active ? ui::kColorSysOnTonalContainer : ui::kColorIcon;
    GetButton(segment)->SetImageModel(
        views::Button::STATE_NORMAL,
        ui::ImageModel::FromVectorIcon(icon, color_id, icon_size));
  };

  update_button_icon(Segment::kTabStrip, features::IsRoundedIconsEnabled()
                                             ? kTabIcon
                                             : kTabOldIcon);
  update_button_icon(Segment::kOrganizer, features::IsRoundedIconsEnabled()
                                              ? kManageSearchIcon
                                              : kTabSearchTabStripOldIcon);
}

void VerticalTabStripSegmentedControl::UpdateSegmentBackgrounds() {
  const ui::ColorId active_bg_color =
      GetWidget() && GetWidget()->ShouldPaintAsActive()
          ? ui::kColorFrameActive
          : ui::kColorFrameInactive;
  auto update_segment_background = [&](Segment segment) {
    if (views::ImageButton* button = GetButton(segment)) {
      button->SetBackground(active_segment_ == segment
                                ? views::CreatePillBackground(active_bg_color)
                                : nullptr);
    }
  };

  update_segment_background(Segment::kTabStrip);
  update_segment_background(Segment::kOrganizer);
}

void VerticalTabStripSegmentedControl::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::mojom::MenuSourceType source_type) {
  if (source != organizer_button_ || !GetWidget()) {
    return;
  }

  menu_runner_.reset();
  menu_model_adapter_.reset();
  menu_model_.reset();

  PrefService* prefs = browser_->GetProfile()->GetPrefs();
  const std::string_view pref_name = prefs::kTabSearchPinnedToTabstrip;
  const bool is_pinned = prefs->GetBoolean(pref_name);
  const int command_id = IDC_TAB_SEARCH_TOGGLE_PIN;
  const int string_id = is_pinned ? IDS_TAB_SEARCH_BUTTON_CXMENU_UNPIN
                                  : IDS_TAB_SEARCH_BUTTON_CXMENU_PIN;
  const gfx::VectorIcon& icon =
      is_pinned
          ? (features::IsRoundedIconsEnabled() ? kKeepOffIcon : kKeepOffOldIcon)
          : (features::IsRoundedIconsEnabled() ? kKeepIcon : kKeepOldIcon);

  menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model_->AddItemWithStringIdAndIcon(
      command_id, string_id,
      ui::ImageModel::FromVectorIcon(icon, ui::kColorIcon, 16));
  menu_model_->SetElementIdentifierAt(0, kTabSearchUnpinMenuItem);

  menu_model_adapter_ = std::make_unique<views::MenuModelAdapter>(
      menu_model_.get(),
      base::BindRepeating(&VerticalTabStripSegmentedControl::OnMenuClosed,
                          base::Unretained(this)));
  std::unique_ptr<views::MenuItemView> root = menu_model_adapter_->CreateMenu();
  menu_runner_ = std::make_unique<views::MenuRunner>(
      std::move(root),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  if (browser_view && browser_view->tab_strip_view()) {
    expand_on_hover_lock_ =
        browser_view->tab_strip_view()->GetExpandOnHoverLock(
            ExpandOnHoverLockType::kKeepCurrentState);
  }

  menu_runner_->RunMenuAt(GetWidget(), nullptr,
                          source->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopLeft, source_type);
}

void VerticalTabStripSegmentedControl::ExecuteCommand(int command_id,
                                                      int event_flags) {
  if (command_id == IDC_TAB_SEARCH_TOGGLE_PIN) {
    chrome::ExecuteCommand(browser_, command_id);
  }
}

void VerticalTabStripSegmentedControl::OnMenuClosed() {
  expand_on_hover_lock_.reset();
  menu_runner_.reset();
}

BEGIN_METADATA(VerticalTabStripSegmentedControl)
END_METADATA
