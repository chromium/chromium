// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <algorithm>
#include <memory>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The max width prevents the popup from growing too much when the password
// field is too long.
constexpr int kPasswordGenerationMaxWidth = 480;

}  // namespace

// Class that shows the generated password and associated UI (currently an
// explanatory text).
class PasswordGenerationPopupViewViews::GeneratedPasswordBox
    : public views::View {
 public:
  GeneratedPasswordBox() = default;
  ~GeneratedPasswordBox() override = default;

  // Fills the view with strings provided by |controller|.
  void Init(PasswordGenerationPopupController* controller);

  void UpdatePassword(const base::string16& password) {
    password_label_->SetText(password);
  }

  void UpdateBackground(SkColor color) {
    SetBackground(views::CreateSolidBackground(color));
    // Setting a background color on the labels may change the text color to
    // improve contrast.
    password_label_->SetBackgroundColor(color);
    suggestion_label_->SetBackgroundColor(color);
  }

  void reset_controller() { controller_ = nullptr; }

 private:
  // Implements the View interface.
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Construct a ColumnSet with one view on the left and another on the right.
  static void BuildColumnSet(views::GridLayout* layout);

  views::Label* suggestion_label_ = nullptr;
  views::Label* password_label_ = nullptr;
  PasswordGenerationPopupController* controller_ = nullptr;
};

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::Init(
    PasswordGenerationPopupController* controller) {
  controller_ = controller;
  views::GridLayout* layout =
      SetLayoutManager(std::make_unique<views::GridLayout>());
  BuildColumnSet(layout);
  layout->StartRow(views::GridLayout::kFixedSize, 0);

  suggestion_label_ = layout->AddView(std::make_unique<views::Label>(
      controller_->SuggestedText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      controller_->state() ==
              PasswordGenerationPopupController::kOfferGeneration
          ? views::style::STYLE_PRIMARY
          : views::style::STYLE_SECONDARY));

  DCHECK(!password_label_);
  password_label_ = layout->AddView(std::make_unique<views::Label>(
      controller_->password(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      STYLE_SECONDARY_MONOSPACED));
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseEntered(
    const ui::MouseEvent& event) {
  if (controller_)
    controller_->SetSelected();
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseExited(
    const ui::MouseEvent& event) {
  if (controller_)
    controller_->SelectionCleared();
}

bool PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMousePressed(
    const ui::MouseEvent& event) {
  return event.GetClickCount() == 1;
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnMouseReleased(
    const ui::MouseEvent& event) {
  if (event.IsOnlyLeftMouseButton() && controller_)
    controller_->PasswordAccepted();
}

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::OnGestureEvent(
    ui::GestureEvent* event) {
  if (!controller_)
    return;
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      controller_->SetSelected();
      break;
    case ui::ET_GESTURE_TAP:
      controller_->PasswordAccepted();
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
    case ui::ET_GESTURE_END:
      controller_->SelectionCleared();
      break;
    default:
      return;
  }
}

// static
void PasswordGenerationPopupViewViews::GeneratedPasswordBox::BuildColumnSet(
    views::GridLayout* layout) {
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                        0 /* resize_percent */,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
  column_set->AddPaddingColumn(
      0 /* resize_percent */,
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL));
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                        1.0 /* resize_percent */,
                        views::GridLayout::ColumnSize::kUsePreferred, 0, 0);
}

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  CreateLayoutAndChildren();
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

void PasswordGenerationPopupViewViews::Show() {
  DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  password_view_->reset_controller();

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateState() {
  RemoveAllChildViews(true);
  password_view_ = nullptr;
  help_label_ = nullptr;
  CreateLayoutAndChildren();
}

void PasswordGenerationPopupViewViews::UpdatePasswordValue() {
  password_view_->UpdatePassword(controller_->password());
  Layout();
}

void PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  if (controller_->password_selected())
    NotifyAXSelection(this);

  password_view_->UpdateBackground(controller_->password_selected()
                                       ? GetSelectedBackgroundColor()
                                       : GetBackgroundColor());
  SchedulePaint();
}

void PasswordGenerationPopupViewViews::CreateLayoutAndChildren() {
  // Add 1px distance between views for the separator.
  views::BoxLayout* box_layout =
      SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical, gfx::Insets(), 1));
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  const int kVerticalPadding =
      provider->GetDistanceMetric(DISTANCE_TOAST_LABEL_VERTICAL);
  const int kHorizontalMargin =
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);

  password_view_ = new GeneratedPasswordBox();
  password_view_->SetBorder(
      views::CreateEmptyBorder(kVerticalPadding, kHorizontalMargin,
                               kVerticalPadding, kHorizontalMargin));
  password_view_->Init(controller_);
  AddChildView(password_view_);
  PasswordSelectionUpdated();

  help_label_ = new views::Label(controller_->HelpText(),
                                 views::style::CONTEXT_DIALOG_BODY_TEXT,
                                 views::style::STYLE_SECONDARY);
  help_label_->SetMultiLine(true);
  help_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  help_label_->SetBorder(
      views::CreateEmptyBorder(kVerticalPadding, kHorizontalMargin,
                               kVerticalPadding, kHorizontalMargin));
  AddChildView(help_label_);
}

void PasswordGenerationPopupViewViews::OnThemeChanged() {
  autofill::AutofillPopupBaseView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetBackgroundColor()));
  password_view_->UpdateBackground(controller_->password_selected()
                                       ? GetSelectedBackgroundColor()
                                       : GetBackgroundColor());
  help_label_->SetBackgroundColor(GetFooterBackgroundColor());
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  gfx::Rect divider_bounds(0, password_view_->bounds().bottom(),
                           password_view_->width(), 1);
  canvas->FillRect(divider_bounds, GetSeparatorColor());
}

void PasswordGenerationPopupViewViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->SetName(
      base::JoinString({controller_->SuggestedText(), controller_->password()},
                       base::ASCIIToUTF16(" ")));
  node_data->SetDescription(controller_->HelpText());
  node_data->role = ax::mojom::Role::kMenuItem;
}

gfx::Size PasswordGenerationPopupViewViews::CalculatePreferredSize() const {
  int width =
      std::max(password_view_->GetPreferredSize().width(),
               gfx::ToEnclosingRect(controller_->element_bounds()).width());
  width = std::min(width, kPasswordGenerationMaxWidth);
  return gfx::Size(width, GetHeightForWidth(width));
}

PasswordGenerationPopupView* PasswordGenerationPopupView::Create(
    PasswordGenerationPopupController* controller) {
  if (!controller->container_view())
    return nullptr;

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}
