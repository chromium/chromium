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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
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
    base::OnceClosure close_callback,
    const std::optional<std::u16string>& action_button_text,
    base::OnceClosure action_button_closure)
    : text(text),
      icon(std::nullopt),
      action_button_text(action_button_text),
      action_button_closure(std::move(action_button_closure)),
      close_callback(std::move(close_callback)) {}

PasswordChangeToast::ToastOptions::ToastOptions(
    const std::u16string& text,
    const gfx::VectorIcon& icon,
    base::OnceClosure close_callback,
    const std::optional<std::u16string>& action_button_text,
    base::OnceClosure action_button_closure)
    : text(text),
      icon(icon),
      action_button_text(action_button_text),
      action_button_closure(std::move(action_button_closure)),
      close_callback(std::move(close_callback)) {}

PasswordChangeToast::ToastOptions::~ToastOptions() = default;
PasswordChangeToast::ToastOptions::ToastOptions(
    PasswordChangeToast::ToastOptions&& other) noexcept = default;
PasswordChangeToast::ToastOptions& PasswordChangeToast::ToastOptions::operator=(
    PasswordChangeToast::ToastOptions&& other) noexcept = default;

PasswordChangeToast::PasswordChangeToast(ToastOptions toast_configuration) {
  SetProperty(views::kElementIdentifierKey, kPasswordChangeViewId);

  // FlexLayout lets the toast compress itself in narrow browser windows.
  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal);

  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                             DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS)));

  auto* throbber_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  throbber_container->SetCrossAxisAlignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  throbber_container->SetProperty(
      views::kMarginsKey,
      gfx::Insets::VH(0, layout_provider->GetDistanceMetric(
                             DISTANCE_TOAST_BUBBLE_LEADING_ICON_SIDE_MARGINS)));
  throbber_ =
      throbber_container->AddChildView(std::make_unique<views::Throbber>(
          layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));
  throbber_->Start();

  label_ = AddChildView(std::make_unique<views::Label>(
      toast_configuration.text, CONTEXT_TOAST_BODY_TEXT));
  label_->SetEnabledColor(ui::kColorToastForeground);
  label_->SetMultiLine(false);
  label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label_->SetAllowCharacterBreak(false);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetLineHeight(
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT));
  label_->SetProperty(views::kMarginsKey,
                      GetLeftMargin(layout_provider->GetDistanceMetric(
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
  action_button_->SetStyle(ui::ButtonStyle::kProminent);
  action_button_->SetProperty(views::kElementIdentifierKey,
                              kPasswordChangeActionButton);
  action_button_->SetAppearDisabledInInactiveWidget(false);
  action_button_->SetProperty(
      views::kMarginsKey,
      GetLeftMargin(layout_provider->GetDistanceMetric(
          DISTANCE_TOAST_BUBBLE_BETWEEN_LABEL_ACTION_BUTTON_SPACING)));

  close_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::BindRepeating(&PasswordChangeToast::OnCloseButtonClicked,
                          base::Unretained(this)),
      vector_icons::kCloseChromeRefreshIcon,
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE),
      ui::kColorToastForeground));
  // Override the image button's border with the appropriate icon border size.
  close_button_->SetBorder(
      views::CreateEmptyBorder(layout_provider->GetInsetsMetric(
          views::InsetsMetric::INSETS_VECTOR_IMAGE_BUTTON)));
  views::InstallCircleHighlightPathGenerator(close_button_);
  close_button_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_button_->SetProperty(views::kElementIdentifierKey,
                             kPasswordChangeCloseButton);
  close_button_->SetProperty(views::kMarginsKey,
                             GetLeftMargin(layout_provider->GetDistanceMetric(
                                 DISTANCE_TOAST_BUBBLE_BETWEEN_CHILD_SPACING)));
  close_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOAST_CLOSE_TOOLTIP));
  close_button_->SetVisible(true);

  UpdateLayout(std::move(toast_configuration));
}

PasswordChangeToast::~PasswordChangeToast() = default;

void PasswordChangeToast::UpdateLayout(ToastOptions configuration) {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  icon_ = configuration.icon;
  icon_view_->SetVisible(icon_.has_value());
  if (icon_.has_value() && GetColorProvider()) {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *icon_.value(), GetColorProvider()->GetColor(ui::kColorToastForeground),
        layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_ICON_SIZE)));
  }
  throbber_->SetVisible(!icon_.has_value());

  label_->SetText(configuration.text);

  if (configuration.action_button_text.has_value()) {
    action_button_->SetText(configuration.action_button_text.value());

    // Only set kAlert a11y role when text is not empty, otherwise it triggers
    // a DCHECK in views::RunAccessibilityPaintChecks().
    action_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kAlert);
    action_button_->SetVisible(true);
  } else {
    action_button_->GetViewAccessibility().SetRole(ax::mojom::Role::kButton);
    action_button_->SetVisible(false);
  }

  layout_manager_->SetInteriorMargin(CalculateInteriorMargin());
  GetViewAccessibility().AnnounceText(configuration.text);

  action_button_closure_ = std::move(configuration.action_button_closure);
  close_callback_ = std::move(configuration.close_callback);
}

gfx::Insets PasswordChangeToast::CalculateInteriorMargin() {
  ChromeLayoutProvider* layout_provider = ChromeLayoutProvider::Get();
  bool action_button_visible = action_button_->GetVisible();
  const int max_child_height =
      action_button_visible ? layout_provider->GetDistanceMetric(
                                  DISTANCE_TOAST_BUBBLE_HEIGHT_ACTION_BUTTON)
                            : layout_provider->GetDistanceMetric(
                                  DISTANCE_TOAST_BUBBLE_HEIGHT_CONTENT);
  const int right_margin_token =
      close_button_->GetVisible()
          ? DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_CLOSE_BUTTON
      : action_button_visible ? DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_ACTION_BUTTON
                              : DISTANCE_TOAST_BUBBLE_MARGIN_RIGHT_LABEL;
  const int total_vertical_margins =
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_HEIGHT) -
      max_child_height;
  return gfx::Insets::TLBR(
      total_vertical_margins / 2,
      layout_provider->GetDistanceMetric(DISTANCE_TOAST_BUBBLE_MARGIN_LEFT),
      total_vertical_margins / 2,
      layout_provider->GetDistanceMetric(right_margin_token));
}

void PasswordChangeToast::OnThemeChanged() {
  views::View::OnThemeChanged();

  CHECK(GetColorProvider());
  if (icon_.has_value()) {
    icon_view_->SetImage(ui::ImageModel::FromVectorIcon(
        *icon_.value(), GetColorProvider()->GetColor(ui::kColorToastForeground),
        ChromeLayoutProvider::Get()->GetDistanceMetric(
            DISTANCE_TOAST_BUBBLE_ICON_SIZE)));
  }
}

void PasswordChangeToast::OnActionButtonClicked() {
  std::move(action_button_closure_).Run();
}

void PasswordChangeToast::OnCloseButtonClicked() {
  std::move(close_callback_).Run();
}

BEGIN_METADATA(PasswordChangeToast)
END_METADATA
