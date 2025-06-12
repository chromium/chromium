// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_change/password_change_toast.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/controls/page_switcher_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/window/dialog_client_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

gfx::Insets GetLeftMargin(const int left_margin) {
  return gfx::Insets::TLBR(0, left_margin, 0, 0);
}

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PasswordChangeToast,
                                      kPasswordChangeViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PasswordChangeToast,
                                      kPasswordChangeActionButton);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PasswordChangeToast,
                                      kPasswordChangeCloseButton);

PasswordChangeToast::ToastOptions::ToastOptions(
    const std::u16string& text,
    const std::u16string& action_button_text,
    base::OnceClosure action_button_closure,
    bool has_close_button)
    : text(text),
      icon(std::nullopt),
      action_button_text(action_button_text),
      action_button_closure(std::move(action_button_closure)),
      has_close_button(has_close_button) {}

PasswordChangeToast::ToastOptions::ToastOptions(
    const std::u16string& text,
    const gfx::VectorIcon& icon,
    const std::u16string& action_button_text,
    base::OnceClosure action_button_closure,
    bool has_close_button)
    : text(text),
      icon(icon),
      action_button_text(action_button_text),
      action_button_closure(std::move(action_button_closure)),
      has_close_button(has_close_button) {}

PasswordChangeToast::ToastOptions::~ToastOptions() = default;
PasswordChangeToast::ToastOptions::ToastOptions(
    PasswordChangeToast::ToastOptions&& other) noexcept = default;
PasswordChangeToast::ToastOptions& PasswordChangeToast::ToastOptions::operator=(
    PasswordChangeToast::ToastOptions&& other) noexcept = default;

PasswordChangeToast::PasswordChangeToast(ToastOptions toast_configuration) {
  SetOwnershipOfNewWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  set_use_custom_frame(true);
  SetShowCloseButton(false);
  DialogDelegate::SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  set_corner_radius(ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_TOAST_BUBBLE_HEIGHT));
  SetProperty(views::kElementIdentifierKey, kPasswordChangeViewId);
  SetAccessibleWindowRole(ax::mojom::Role::kAlert);
  SetAccessibleTitle(toast_configuration.text);

  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();

  // FlexLayout lets the toast compress itself in narrow browser windows.
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal);

  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, lp->GetDistanceMetric(
                             DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS)));

  auto* throbber_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  throbber_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  throbber_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, lp->GetDistanceMetric(
                             DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS)));
  throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>(
          lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));
  throbber_->Start();

  label_ = AddChildView(std::make_unique<views::Label>(
      toast_configuration.text, CONTEXT_TOAST_BODY_TEXT));
  label_->SetEnabledColor(ui::kColorToastForeground);
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetLineHeight(
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));
  label_->SetProperty(views::kMarginsKey,
                      GetLeftMargin(lp->GetDistanceMetric(
                          DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING)));
  label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero));

  action_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PasswordChangeToast::OnActionButtonClicked,
                          base::Unretained(this)),
      toast_configuration.action_button_text.value_or(std::u16string())));
  action_button_->SetEnabledTextColors(ui::kColorToastButton);
  action_button_->SetBgColorIdOverride(ui::kColorToastBackgroundProminent);
  action_button_->SetStrokeColorIdOverride(ui::kColorToastButton);
  action_button_->SetPreferredSize(gfx::Size(
      action_button_->GetPreferredSize().width(),
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
  action_button_->SetStyle(ui::ButtonStyle::kProminent);
  action_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
  action_button_->SetProperty(views::kElementIdentifierKey,
                              kPasswordChangeActionButton);
  action_button_->SetAppearDisabledInInactiveWidget(false);
  action_button_->SetProperty(
      views::kMarginsKey,
      GetLeftMargin(lp->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING)));

  close_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PasswordChangeToast::OnCloseButtonClicked,
                          base::Unretained(this)),
      vector_icons::kCloseChromeRefreshIcon,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
      ui::kColorToastForeground));
  // Override the image button's border with the appropriate icon border size.
  close_button_->SetBorder(views::CreateEmptyBorder(
      lp->GetInsetsMetric(views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON)));
  views::InstallCircleHighlightPathGenerator(close_button_);
  close_button_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button_->SetProperty(views::kElementIdentifierKey,
                             kPasswordChangeCloseButton);
  close_button_->SetProperty(views::kMarginsKey,
                             GetLeftMargin(lp->GetDistanceMetric(
                                 DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING)));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOAST_CLOSE_TOOLTIP));

  UpdateConfiguration(std::move(toast_configuration));

  const int total_vertical_margins =
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON);
  set_margins(gfx::Insets::TLBR(
      total_vertical_margins / 2,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins / 2,
      lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON)));
}

PasswordChangeToast::~PasswordChangeToast() = default;

void PasswordChangeToast::UpdateLayout(ToastOptions configuration) {
  UpdateConfiguration(std::move(configuration));
  GetWidget()->SetBounds(GetDesiredWidgetBounds());
}

views::Widget* PasswordChangeToast::GetWidget() {
  return View::GetWidget();
}

const views::Widget* PasswordChangeToast::GetWidget() const {
  return View::GetWidget();
}

views::View* PasswordChangeToast::GetContentsView() {
  return this;
}

ui::mojom::ModalType PasswordChangeToast::GetModalType() const {
  return ui::mojom::ModalType::kChild;
}

void PasswordChangeToast::UpdateConfiguration(ToastOptions configuration) {
  action_button_closure_ = std::move(configuration.action_button_closure);
  ChromeLayoutProvider* lp = ChromeLayoutProvider::Get();
  icon_view_->SetVisible(configuration.icon.has_value());
  if (configuration.icon.has_value()) {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *configuration.icon.value(),
        GetColorProvider()->GetColor(ui::kColorToastForeground),
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));
  }
  throbber_->SetVisible(!configuration.icon.has_value());

  label_->SetText(configuration.text);

  if (configuration.action_button_text.has_value()) {
    action_button_->SetText(configuration.action_button_text.value());
    auto tmp_button = std::make_unique<views::MdTextButton>(
        views::Button::PressedCallback(),
        configuration.action_button_text.value_or(std::u16string()));
    // Even though the text might change
    // action_button_->GetPreferredSize().width() is not updated properly, this
    // is why a temporary button is created to set preferred size manually.
    action_button_->SetPreferredSize(gfx::Size(
        tmp_button->GetPreferredSize().width(),
        lp->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)));
  }
  action_button_->SetVisible(configuration.action_button_text.has_value());
  close_button_->SetVisible(configuration.has_close_button);
}

void PasswordChangeToast::AddedToWidget() {
  views::BubbleFrameView* frame_view = GetBubbleFrameView();
  if (frame_view) {
    frame_view->SetBackgroundColor(ui::kColorToastBackgroundProminent);
    frame_view->bubble_border()->set_draw_border_stroke(false);
  }
}

void PasswordChangeToast::OnCloseButtonClicked() {
  GetWidget()->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
}
void PasswordChangeToast::OnActionButtonClicked() {
  std::move(action_button_closure_).Run();
}

BEGIN_METADATA(PasswordChangeToast)
END_METADATA
