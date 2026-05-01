// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/ai_overlay_toolbar_button.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/ai_overlay_dialog/ai_overlay_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/toolbar/pinned_toolbar_actions_container.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"

namespace {

constexpr int kOptionsIconSize = 16;
constexpr int kOptionsIconSizeTouch = 24;

constexpr int kExtraWidth = 12;
constexpr int kExtraWidthTouch = 24;

void OnCaptionsToggled(ttc::AiOverlayDialogController* controller) {
  controller->set_captions_visible(!controller->captions_visible());
}

void OnPersonaToggled(ttc::AiOverlayDialogController* controller) {
  controller->set_use_persona(!controller->use_persona());
}

}  // namespace

AiOverlayToolbarButton::AiOverlayToolbarButton(
    Browser* browser,
    actions::ActionId action_id,
    base::WeakPtr<PinnedToolbarActionsContainer> container)
    : PinnedActionToolbarButton(browser, action_id, container) {
  options_button_ = AddChildView(views::CreateVectorImageButton(
      base::BindRepeating(&AiOverlayToolbarButton::OnOptionsButtonPressed,
                          base::Unretained(this))));
  options_button_->SetVisible(false);
  options_button_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_SETTINGS));
  options_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_SETTINGS));
}

AiOverlayToolbarButton::~AiOverlayToolbarButton() = default;

void AiOverlayToolbarButton::Layout(PassKey pass_key) {
  LayoutSuperclass<PinnedActionToolbarButton>(this);
  if (overlay_active_) {
    gfx::Rect bounds = GetLocalBounds();

    const bool touch_ui = ui::TouchUiController::Get()->touch_ui();

    // Position options button at the right.
    int options_size = touch_ui ? kOptionsIconSizeTouch : kOptionsIconSize;
    int right_margin = 0;
    int options_x = bounds.width() - options_size - right_margin;
    int options_y = (bounds.height() - options_size) / 2;
    options_button_->SetBounds(options_x, options_y, options_size,
                               options_size);
    options_button_->SetVisible(true);

    // Shift the main icon to the left.
    // We want it centered in the left part.
    // The extra width added in CalculatePreferredSize is kExtraWidthTouch in
    // touch UI, kExtraWidth otherwise. To center the icon in the original
    // button width, we shift left by half the extra width.
    int extra_width = touch_ui ? kExtraWidthTouch : kExtraWidth;
    int shift = extra_width / 2;
    views::View* cv = image_container_view();
    gfx::Rect cv_bounds = cv->bounds();
    cv->SetBounds(cv_bounds.x() - shift, cv_bounds.y(), cv_bounds.width(),
                  cv_bounds.height());

  } else {
    options_button_->SetVisible(false);
  }
}

gfx::Size AiOverlayToolbarButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  gfx::Size size =
      PinnedActionToolbarButton::CalculatePreferredSize(available_size);
  if (overlay_active_) {
    int extra_width = ui::TouchUiController::Get()->touch_ui()
                          ? kExtraWidthTouch
                          : kExtraWidth;
    size.set_width(size.width() + extra_width);
  }
  return size;
}

void AiOverlayToolbarButton::UpdateIcon() {
  PinnedActionToolbarButton::UpdateIcon();
  if (GetWidget()) {
    const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
    const gfx::VectorIcon& icon =
        touch_ui ? kBrowserToolsTouchIcon : kBrowserToolsIcon;
    int icon_size = touch_ui ? kOptionsIconSizeTouch : kOptionsIconSize;
    views::SetImageFromVectorIconWithColor(
        options_button_, icon, icon_size,
        views::IconColors(
            GetColorProvider()->GetColor(ui::kColorIcon),
            GetColorProvider()->GetColor(ui::kColorIconDisabled)));
  }
}

void AiOverlayToolbarButton::SetOverlayActive(bool active) {
  if (overlay_active_ == active) {
    return;
  }
  overlay_active_ = active;
  PreferredSizeChanged();
}

void AiOverlayToolbarButton::OnOptionsButtonPressed() {
  auto* controller = ttc::AiOverlayDialogController::From(browser());
  if (!controller) {
    return;
  }

  // Reduce padding by half.
  gfx::Insets dialog_insets =
      ChromeLayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
  dialog_insets.set_top(dialog_insets.top() / 2);
  dialog_insets.set_bottom(dialog_insets.bottom() / 2);
  dialog_insets.set_left(dialog_insets.left() / 2);
  dialog_insets.set_right(dialog_insets.right() / 2);

  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegate>(
      options_button_, views::BubbleBorder::TOP_RIGHT);
  bubble_delegate->SetShowTitle(false);
  bubble_delegate->SetShowCloseButton(false);
  bubble_delegate->SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  bubble_delegate->set_close_on_deactivate(true);
  bubble_delegate->set_margins(dialog_insets);

  auto contents_view = std::make_unique<views::TableLayoutView>();

  // We want the dialog to be around 200px wide. The toggle is ~40px.
  // We set the first column to a min-width of 130px.
  // This avoids hardcoding set_fixed_width() which can cause height truncation
  // bugs.
  contents_view->AddColumn(
      views::LayoutAlignment::kStart, views::LayoutAlignment::kCenter, 1.0f,
      views::TableLayout::ColumnSize::kUsePreferred, 0, 130);
  contents_view->AddColumn(views::LayoutAlignment::kEnd,
                           views::LayoutAlignment::kCenter, 0.0f,
                           views::TableLayout::ColumnSize::kUsePreferred, 0, 0);

  // Captions row
  contents_view->AddRows(1, views::TableLayout::kFixedSize);

  auto* captions_label =
      contents_view->AddChildView(std::make_unique<views::Label>(u"Captions"));
  captions_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* captions_toggle =
      contents_view->AddChildView(std::make_unique<views::ToggleButton>(
          base::BindRepeating(&OnCaptionsToggled, controller)));
  captions_toggle->SetIsOn(controller->captions_visible());
  captions_toggle->GetViewAccessibility().SetName(u"Captions");

  // Spacing between rows
  contents_view->AddPaddingRow(0,
                               ChromeLayoutProvider::Get()->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_VERTICAL) /
                                   2);

  // Persona row
  contents_view->AddRows(1, views::TableLayout::kFixedSize);

  auto* persona_label =
      contents_view->AddChildView(std::make_unique<views::Label>(u"Persona"));
  persona_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  auto* persona_toggle =
      contents_view->AddChildView(std::make_unique<views::ToggleButton>(
          base::BindRepeating(&OnPersonaToggled, controller)));
  persona_toggle->SetIsOn(controller->use_persona());
  persona_toggle->GetViewAccessibility().SetName(u"Persona");

  bubble_delegate->SetContentsView(std::move(contents_view));
  views::BubbleDialogDelegate::CreateBubble(
      std::move(bubble_delegate),
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET)
      ->Show();
}

std::unique_ptr<views::ActionViewInterface>
AiOverlayToolbarButton::GetActionViewInterface() {
  return std::make_unique<AiOverlayToolbarButtonActionViewInterface>(this);
}

AiOverlayToolbarButtonActionViewInterface::
    AiOverlayToolbarButtonActionViewInterface(
        AiOverlayToolbarButton* action_view)
    : PinnedActionToolbarButtonActionViewInterface(action_view),
      action_view_(action_view) {}

void AiOverlayToolbarButtonActionViewInterface::ActionItemChangedImpl(
    actions::ActionItem* action_item) {
  PinnedActionToolbarButtonActionViewInterface::ActionItemChangedImpl(
      action_item);
  action_view_->SetOverlayActive(
      action_item->GetProperty(ttc::kActionAiOverlayActiveKey));
}

BEGIN_METADATA(AiOverlayToolbarButton)
END_METADATA
