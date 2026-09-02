// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_app_loading_view.h"

#include <memory>

#include "base/feature_list.h"
#include "base/task/sequenced_task_runner.h"
#include "components/payments/core/features.h"
#include "components/payments/core/sizes.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

constexpr base::TimeDelta kLoadingMessageShowDelay = base::Milliseconds(500);
constexpr base::TimeDelta kMinimumMessageShowDuration = base::Milliseconds(500);

}  // namespace

namespace payments {

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PaymentAppLoadingView, kTopViewId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(PaymentAppLoadingView, kCloseButtonId);

PaymentAppLoadingView::PaymentAppLoadingView(
    const SkBitmap* icon,
    const GURL& app_origin,
    const GURL& top_origin,
    views::Button::PressedCallback close_callback) {
  SetProperty(views::kElementIdentifierKey, kTopViewId);
  // Configure ACCESSIBLE_ONLY focus behavior so screen readers can announce the
  // loading dialog context during accessibility mode.
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_PAYMENT_APP_LOADING_TITLE),
      ax::mojom::NameFrom::kAttribute);
  // Paint to a layer so this view correctly overlays ViewStack (which also
  // paints to a layer).
  SetPaintToLayer();
  // Set a solid background to hide underlying dialog content.
  SetBackground(views::CreateSolidBackground(ui::kColorDialogBackground));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  const std::u16string formatted_app_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(app_origin),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
  const std::u16string formatted_top_origin =
      url_formatter::FormatOriginForSecurityDisplay(
          url::Origin::Create(top_origin),
          url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);

  header_view_ = AddChildView(std::make_unique<views::View>());
  std::unique_ptr<views::View> icon_view;
  if (base::FeatureList::IsEnabled(features::kPaymentHandlerCameraAccessUx)) {
    icon_view = CreatePaymentHandlerLoadingIconView();
  }
  PaymentHandlerHeaderViews header_views = PopulatePaymentHandlerHeaderView(
      header_view_, std::move(icon_view), icon, formatted_app_origin,
      std::move(close_callback));
  origin_label_ = header_views.origin_label.get();
  origin_label_->SetText(formatted_app_origin);
  // We only need one accessibility name on loading view for screen reader to
  // announce.
  origin_label_->SetFocusBehavior(FocusBehavior::NEVER);
  close_button_ = header_views.close_button.get();
  if (close_button_) {
    close_button_->SetProperty(views::kElementIdentifierKey, kCloseButtonId);
  }

  progress_bar_ = AddChildView(std::make_unique<PaymentHandlerProgressBar>());
  progress_bar_->SetValue(-1.0);

  loading_message_label_ = AddChildView(std::make_unique<views::Label>());
  loading_message_label_->SetMultiLine(true);
  loading_message_label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  loading_message_label_->SetTextContext(
      views::style::CONTEXT_DIALOG_BODY_TEXT);
  loading_message_label_->SetTextStyle(views::style::STYLE_BODY_3_MEDIUM);
  loading_message_label_->SetText(
      l10n_util::GetStringFUTF16(IDS_PAYMENT_APP_LOADING_MESSAGE,
                                 formatted_app_origin, formatted_top_origin));
  gfx::Insets dialog_insets =
      views::LayoutProvider::Get()->GetInsetsMetric(views::INSETS_DIALOG);
  loading_message_label_->SetProperty(
      views::kMarginsKey,
      gfx::Insets::TLBR(/*top=*/kPaymentAppLoadingViewMessageTopInset,
                        /*left=*/dialog_insets.left(),
                        /*bottom=*/0,
                        /*right=*/dialog_insets.right()));
  // The loading message is hidden by default and only shown after 500ms.
  loading_message_label_->SetVisible(false);
  // Safe to use `base::Unretained(this)` because `delay_message_timer_` is
  // owned by `this` and will be destroyed (canceling any pending callback) if
  // `this` is destroyed.
  delay_message_timer_.Start(
      FROM_HERE, kLoadingMessageShowDelay,
      base::BindOnce(&PaymentAppLoadingView::ShowLoadingMessage,
                     base::Unretained(this)));
}

PaymentAppLoadingView::~PaymentAppLoadingView() = default;

void PaymentAppLoadingView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // SetHeaderColors requires a valid Widget and ColorProvider to properly
  // calculate the background and contrasting element colors (e.g., close button
  // and origin label). During construction, the view is not yet attached to a
  // widget, causing color resolution to fall back to placeholder values.
  // Calling SetHeaderColors here on theme change ensures the proper
  // ColorProvider is available and updates dynamically if theme changes.
  // Note that web content is different between loading view and web flow view,
  // there is no way to ensure the color setting match exactly, passing
  // std::nullopt for theme_color aligns the loading view's default header
  // appearance with the web flow view when no explicit theme color is set by
  // the payment handler.
  SetHeaderColors(header_view_, origin_label_, progress_bar_, close_button_,
                  /*theme_color=*/std::nullopt);
}

void PaymentAppLoadingView::Hide(base::OnceClosure on_hidden_callback) {
  // If the message was never shown, cancel the 500ms delay timer and hide.
  if (!message_shown_time_.has_value()) {
    delay_message_timer_.Stop();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_hidden_callback));
    return;
  }

  // If the message was shown, calculate remaining time to reach 500ms
  // minimum display duration.
  base::TimeDelta elapsed = base::TimeTicks::Now() - *message_shown_time_;
  if (elapsed >= kMinimumMessageShowDuration) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_hidden_callback));
    return;
  }

  // Safety guard to prevent re-entry of Hide() if hide timer is already
  // running.
  if (hide_timer_.IsRunning()) {
    return;
  }
  hide_timer_.Start(FROM_HERE, kMinimumMessageShowDuration - elapsed,
                    std::move(on_hidden_callback));
}

void PaymentAppLoadingView::ShowLoadingMessage() {
  if (loading_message_label_) {
    loading_message_label_->SetVisible(true);
    message_shown_time_ = base::TimeTicks::Now();
  }
}

BEGIN_METADATA(PaymentAppLoadingView)
END_METADATA

}  // namespace payments
