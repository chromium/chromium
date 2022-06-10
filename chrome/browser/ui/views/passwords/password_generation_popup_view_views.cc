// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_generation_popup_view_views.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_generation_popup_controller.h"
#include "chrome/browser/ui/passwords/ui_utils.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/passwords/views_utils.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

// The max width prevents the popup from growing too much when the password
// field is too long.
constexpr int kPasswordGenerationMaxWidth = 480;

// The default icon size used in the password generation drop down.
constexpr int kIconSize = 16;

}  // namespace

// Class that shows the generated password and associated UI (currently an
// explanatory text).
class PasswordGenerationPopupViewViews::GeneratedPasswordBox
    : public views::View {
 public:
  METADATA_HEADER(GeneratedPasswordBox);
  GeneratedPasswordBox() = default;
  ~GeneratedPasswordBox() override = default;

  // Fills the view with strings provided by |controller|.
  void Init(base::WeakPtr<PasswordGenerationPopupController> controller);

  void UpdatePassword(const std::u16string& password) {
    password_label_->SetText(password);
  }

  void UpdateBackground(SkColor color) {
    SetBackground(views::CreateSolidBackground(color));
    // Setting a background color on the labels may change the text color to
    // improve contrast.
    password_label_->SetBackgroundColor(color);
    suggestion_label_->SetBackgroundColor(color);
  }

  void AddSpacerWithSize(int spacer_width,
                         bool resize,
                         views::BoxLayout* layout) {
    auto spacer = std::make_unique<views::View>();
    spacer->SetPreferredSize(gfx::Size(spacer_width, 1));
    layout->SetFlexForView(AddChildView(std::move(spacer)),
                           /*flex=*/resize ? 1 : 0,
                           /*use_min_size=*/true);
  }

  void reset_controller() { controller_ = nullptr; }

 private:
  // Implements the View interface.
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  raw_ptr<views::Label> suggestion_label_ = nullptr;
  raw_ptr<views::Label> password_label_ = nullptr;
  base::WeakPtr<PasswordGenerationPopupController> controller_ = nullptr;
};

void PasswordGenerationPopupViewViews::GeneratedPasswordBox::Init(
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  controller_ = controller;
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  if (base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerDesktop)) {
    AddChildView(
        std::make_unique<views::ImageView>(ui::ImageModel::FromVectorIcon(
            GooglePasswordManagerVectorIcon(), ui::kColorIcon, kIconSize)));
    AddSpacerWithSize(AutofillPopupBaseView::GetHorizontalPadding(),
                      /*resize=*/false, layout);
  }

  suggestion_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->SuggestedText(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      controller_->state() ==
              PasswordGenerationPopupController::kOfferGeneration
          ? views::style::STYLE_PRIMARY
          : views::style::STYLE_SECONDARY));

  AddSpacerWithSize(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          DISTANCE_BETWEEN_PRIMARY_AND_SECONDARY_LABELS_HORIZONTAL),
      /*resize=*/true, layout);

  DCHECK(!password_label_);
  password_label_ = AddChildView(std::make_unique<views::Label>(
      controller_->password(), views::style::CONTEXT_DIALOG_BODY_TEXT,
      STYLE_SECONDARY_MONOSPACED));
  layout->SetFlexForView(password_label_, 1);
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

BEGIN_METADATA(PasswordGenerationPopupViewViews,
               GeneratedPasswordBox,
               views::View)
END_METADATA

PasswordGenerationPopupViewViews::PasswordGenerationPopupViewViews(
    base::WeakPtr<PasswordGenerationPopupController> controller,
    views::Widget* parent_widget)
    : AutofillPopupBaseView(controller, parent_widget),
      controller_(controller) {
  CreateLayoutAndChildren();
}

PasswordGenerationPopupViewViews::~PasswordGenerationPopupViewViews() = default;

bool PasswordGenerationPopupViewViews::Show() {
  return DoShow();
}

void PasswordGenerationPopupViewViews::Hide() {
  // The controller is no longer valid after it hides us.
  controller_ = nullptr;
  password_view_->reset_controller();

  DoHide();
}

void PasswordGenerationPopupViewViews::UpdateState() {
  RemoveAllChildViews();
  password_view_ = nullptr;
  help_label_ = nullptr;
  CreateLayoutAndChildren();
}

void PasswordGenerationPopupViewViews::UpdatePasswordValue() {
  password_view_->UpdatePassword(controller_->password());
  Layout();
}

bool PasswordGenerationPopupViewViews::UpdateBoundsAndRedrawPopup() {
  return DoUpdateBoundsAndRedrawPopup();
}

void PasswordGenerationPopupViewViews::PasswordSelectionUpdated() {
  if (controller_->password_selected())
    NotifyAXSelection(this);

  if (!GetWidget())
    return;

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
  password_view_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets::TLBR(kVerticalPadding, kHorizontalMargin, kVerticalPadding,
                        kHorizontalMargin)));
  password_view_->Init(controller_);
  AddChildView(password_view_.get());
  PasswordSelectionUpdated();

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kUnifiedPasswordManagerDesktop)) {
    help_label_ = new views::Label(controller_->HelpText(),
                                   views::style::CONTEXT_DIALOG_BODY_TEXT,
                                   views::style::STYLE_SECONDARY);
    help_label_->SetMultiLine(true);
    help_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    help_label_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kVerticalPadding, kHorizontalMargin, kVerticalPadding,
                          kHorizontalMargin)));
    AddChildView(help_label_.get());
  } else {
    base::RepeatingClosure open_password_manager_closure = base::BindRepeating(
        [](PasswordGenerationPopupViewViews* view) {
          if (!view->controller_)
            return;
          view->controller_->OnGooglePasswordManagerLinkClicked();
        },
        base::Unretained(this));

    help_styled_label_ = AddChildView(CreateGooglePasswordManagerLabel(
        /*text_message_id=*/
        IDS_PASSWORD_GENERATION_PROMPT_GOOGLE_PASSWORD_MANAGER,
        /*link_message_id=*/
        IDS_PASSWORD_BUBBLES_PASSWORD_MANAGER_LINK_TEXT_SYNCED_TO_ACCOUNT,
        controller_->GetPrimaryAccountEmail(), open_password_manager_closure));

    help_styled_label_->SetBorder(views::CreateEmptyBorder(
        gfx::Insets::TLBR(kVerticalPadding, kHorizontalMargin, kVerticalPadding,
                          kHorizontalMargin)));
  }
}

void PasswordGenerationPopupViewViews::OnThemeChanged() {
  autofill::AutofillPopupBaseView::OnThemeChanged();
  SetBackground(views::CreateSolidBackground(GetBackgroundColor()));
  password_view_->UpdateBackground(controller_->password_selected()
                                       ? GetSelectedBackgroundColor()
                                       : GetBackgroundColor());
  if (help_label_) {
    help_label_->SetBackgroundColor(GetFooterBackgroundColor());
  } else if (help_styled_label_) {
    help_styled_label_->SetDisplayedOnBackgroundColor(
        GetFooterBackgroundColor());
  }
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
  canvas->FillRect(divider_bounds,
                   GetColorProvider()->GetColor(GetSeparatorColorId()));
}

void PasswordGenerationPopupViewViews::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  if (!controller_) {
    return;
  }
  node_data->SetName(base::JoinString(
      {controller_->SuggestedText(), controller_->password()}, u" "));
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
    base::WeakPtr<PasswordGenerationPopupController> controller) {
  if (!controller->container_view())
    return nullptr;

  views::Widget* observing_widget =
      views::Widget::GetTopLevelWidgetForNativeView(
          controller->container_view());

  return new PasswordGenerationPopupViewViews(controller, observing_widget);
}
