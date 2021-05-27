// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/arc_splash_screen_dialog_view.h"

#include "base/bind.h"
#include "components/arc/compat_mode/overlay_dialog.h"
#include "components/arc/compat_mode/style/arc_color_provider.h"
#include "components/arc/vector_icons/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace arc {

using ClickedCallback = base::RepeatingCallback<void()>;

namespace {

std::unique_ptr<views::BubbleBorder> CreateBorder(SkColor background_color) {
  constexpr int kCornerRadius = 12;
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::STANDARD_SHADOW,
      background_color);
  border->SetCornerRadius(kCornerRadius);
  return border;
}

std::unique_ptr<views::Button> CreateCloseButton(
    views::Button::PressedCallback close_callback) {
  constexpr gfx::Size kCloseButtonSize{32, 32};
  auto close_button = views::CreateVectorImageButton(std::move(close_callback));
  views::SetImageFromVectorIconWithColor(
      close_button.get(), vector_icons::kCloseRoundedIcon,
      GetContentLayerColor(ContentLayerType::kIconColorPrimary));
  close_button->SetSize(kCloseButtonSize);
  close_button->ink_drop()->SetMode(views::InkDropHost::InkDropMode::OFF);
  close_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_APP_ACCNAME_CLOSE));
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  return close_button;
}

std::unique_ptr<views::View> CreateMessageBox(ClickedCallback link_callback) {
  constexpr int kMessageBoxSpacing = 16;
  auto message_box_view = std::make_unique<views::BoxLayoutView>();
  message_box_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  message_box_view->SetMainAxisAlignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  message_box_view->SetBetweenChildSpacing(kMessageBoxSpacing);

  // message title
  const std::u16string heading =
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE);
  auto heading_label = std::make_unique<views::Label>(
      heading, views::style::CONTEXT_DIALOG_TITLE);
  heading_label->SetMultiLine(true);
  heading_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  heading_label->SetAllowCharacterBreak(true);
  message_box_view->AddChildView(std::move(heading_label));

  // message body
  const std::u16string link =
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_LINK);
  size_t offset;
  const std::u16string text = l10n_util::GetStringFUTF16(
      IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_BODY, link, &offset);
  auto body_label = std::make_unique<views::StyledLabel>();
  body_label->SetText(text);
  body_label->SetDefaultTextStyle(views::style::STYLE_SECONDARY);
  body_label->SetTextContext(
      views::style::TextContext::CONTEXT_DIALOG_BODY_TEXT);
  body_label->SetHorizontalAlignment(gfx::ALIGN_CENTER);

  // Create link portion
  views::StyledLabel::RangeStyleInfo link_style;
  auto link_view = std::make_unique<views::Link>(link);
  link_view->SetCallback(std::move(link_callback));
  link_view->SetEnabledColor(gfx::kGoogleBlue600);
  link_view->SetTextStyle(views::style::STYLE_SECONDARY);
  link_view->SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT);
  link_style.custom_view = link_view.get();
  body_label->AddCustomView(std::move(link_view));
  body_label->AddStyleRange(gfx::Range(offset, offset + link.length()),
                            link_style);
  message_box_view->AddChildView(std::move(body_label));
  return message_box_view;
}

}  // namespace

ArcSplashScreenDialogView::TestApi::TestApi(ArcSplashScreenDialogView* view)
    : view_(view) {}

ArcSplashScreenDialogView::TestApi::~TestApi() = default;

views::Button* ArcSplashScreenDialogView::TestApi::close_button() const {
  return view_->close_button_;
}

ArcSplashScreenDialogView::ArcSplashScreenDialogView(
    views::Button::PressedCallback close_callback) {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);
  layout->set_inside_border_insets(gfx::Insets(6));

  const auto background_color = GetDialogBackgroundBaseColor();
  auto border = CreateBorder(background_color);
  SetBackground(std::make_unique<views::BubbleBackground>(border.get()));
  SetBorder(std::move(border));

  // add close button
  auto* caption = AddChildView(std::make_unique<views::BoxLayoutView>());
  caption->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  caption->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kEnd);
  auto close_button = CreateCloseButton(std::move(close_callback));
  close_button_ = caption->AddChildView(std::move(close_button));

  // add main view
  constexpr int kImageSpacing = 8;
  constexpr gfx::Insets kImageMargin{0, 18, 26, 18};
  auto* main_view = AddChildView(std::make_unique<views::BoxLayoutView>());
  main_view->SetOrientation(views::BoxLayout::Orientation::kVertical);
  main_view->SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
  main_view->SetInsideBorderInsets(kImageMargin);
  main_view->SetBetweenChildSpacing(kImageSpacing);

  auto image_view = std::make_unique<views::ImageView>();
  constexpr int kLogoImageSize = 122;
  image_view->SetImage(gfx::CreateVectorIcon(kCompatModeSplashscreenIcon,
                                             kLogoImageSize, background_color));
  main_view->AddChildView(std::move(image_view));

  auto message_box = CreateMessageBox(base::BindRepeating(
      &ArcSplashScreenDialogView::OnLinkClicked, base::Unretained(this)));
  main_view->AddChildView(std::move(message_box));
}

ArcSplashScreenDialogView::~ArcSplashScreenDialogView() = default;

gfx::Size ArcSplashScreenDialogView::CalculatePreferredSize() const {
  gfx::Size size = views::View::CalculatePreferredSize();

  // Set the maximum width of this dialog view.
  views::LayoutProvider* provider = views::LayoutProvider::Get();
  size.set_width(provider->GetDistanceMetric(
      views::DistanceMetric::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));
  return size;
}

void ArcSplashScreenDialogView::AddedToWidget() {
  auto& view_ax = GetWidget()->GetRootView()->GetViewAccessibility();
  view_ax.OverrideRole(ax::mojom::Role::kDialog);
  view_ax.OverrideName(
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_SPLASH_SCREEN_TITLE));
}

void ArcSplashScreenDialogView::OnLinkClicked() {
  // TODO(b/180253004): Calling per-app setting
  NOTIMPLEMENTED();
}

void ShowSplashScreenDialog(aura::Window* parent) {
  auto dialog_view = std::make_unique<ArcSplashScreenDialogView>(
      base::BindRepeating(&CloseOverlayDialogIfAny, base::Unretained(parent)));
  ShowOverlayDialog(parent, std::move(dialog_view));
}

}  // namespace arc
