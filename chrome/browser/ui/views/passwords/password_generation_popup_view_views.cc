// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/views/autofill/view_util.h"
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

  // |password| is the generated password, |suggestion| is the text to the left
  // of it. |generating_state| means that the generated password is offered.
  void Init(const base::string16& password,
            const base::string16& suggestion,
            PasswordGenerationPopupController::GenerationUIState state) {
    views::GridLayout* layout =
        SetLayoutManager(std::make_unique<views::GridLayout>());
    BuildColumnSet(layout);
    layout->StartRow(views::GridLayout::kFixedSize, 0);

    layout->AddView(autofill::CreateLabelWithColorReadabilityDisabled(
        suggestion, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
        state == PasswordGenerationPopupController::kOfferGeneration
            ? views::style::STYLE_PRIMARY
            : views::style::STYLE_SECONDARY));

    DCHECK(!password_label_);
    password_label_ =
        layout->AddView(autofill::CreateLabelWithColorReadabilityDisabled(
            password, ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
            STYLE_SECONDARY_MONOSPACED));
  }

  void UpdatePassword(const base::string16& password) {
    password_label_->SetText(password);
  }

  // views::View:
  bool CanProcessEventsWithinSubtree() const override {
    // Send events to the parent view for handling.
    return false;
  }

 private:
  // Construct a ColumnSet with one view on the left and another on the right.
  static void BuildColumnSet(views::GridLayout* layout) {
    views::ColumnSet* column_set = layout->AddColumnSet(0);
    column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                          0 /* resize_percent */, views::GridLayout::USE_PREF,
                          0, 0);
    column_set->AddPaddingColumn(
        0 /* resize_percent */,
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL));
    column_set->AddColumn(views::GridLayout::TRAILING,
                          views::GridLayout::CENTER, 1.0 /* resize_percent */,
                          views::GridLayout::USE_PREF, 0, 0);
  }

  views::Label* password_label_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(GeneratedPasswordBox);
};

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    PasswordGenerationPopupController* controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  CreateLayoutAndChildren();
  SetBackground(views::CreateSolidBackground(GetBackgroundColor()));
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

void PasswordGenerationPopupViewViews::Show() {
  DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = NULL;

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateState() {
  RemoveAllChildViews(true);
  password_view_ = nullptr;
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
  if (!password_view_)
    return;

  if (controller_->password_selected())
    NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);

  password_view_->SetBackground(views::CreateSolidBackground(
      controller_->password_selected() ? GetSelectedBackgroundColor()
                                       : GetBackgroundColor()));
  SchedulePaint();
}

bool PasswordGenerationPopupViewViews::IsPointInPasswordBounds(
    const gfx::Point& point) {
  if (password_view_) {
    gfx::Point point_password_view = point;
    ConvertPointToTarget(this, password_view_, &point_password_view);
    return password_view_->HitTestPoint(point_password_view);
  }
  return false;
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
  password_view_->Init(controller_->password(), controller_->SuggestedText(),
                       controller_->state());
  AddChildView(password_view_);
  PasswordSelectionUpdated();

  views::Label* help_label = new views::Label(
      controller_->HelpText(), ChromeTextContext::CONTEXT_BODY_TEXT_LARGE,
      views::style::STYLE_SECONDARY);
  help_label->SetMultiLine(true);
  help_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  help_label->SetBackground(
      views::CreateSolidBackground(GetFooterBackgroundColor()));
  help_label->SetBorder(
      views::CreateEmptyBorder(kVerticalPadding, kHorizontalMargin,
                               kVerticalPadding, kHorizontalMargin));
  AddChildView(help_label);
}

void PasswordGenerationPopupViewViews::OnPaint(gfx::Canvas* canvas) {
  if (!controller_)
    return;

  // Draw border and background.
  views::View::OnPaint(canvas);

  // Divider line needs to be drawn after OnPaint() otherwise the background
  // will overwrite the divider.
  if (password_view_) {
    gfx::Rect divider_bounds(0, password_view_->bounds().bottom(),
                             password_view_->width(), 1);
    canvas->FillRect(divider_bounds, GetSeparatorColor());
  }
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
