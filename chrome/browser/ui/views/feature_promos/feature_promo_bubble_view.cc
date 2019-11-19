// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"

#include <memory>
#include <utility>

#include "base/metrics/user_metrics.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_timeout.h"
#include "components/variations/variations_associated_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"

namespace {

// The amount of time the promo should stay onscreen if the user
// never hovers over it.
constexpr base::TimeDelta kDelayDefault = base::TimeDelta::FromSeconds(10);

// The amount of time the promo should stay onscreen after the
// user stops hovering over it.
constexpr base::TimeDelta kDelayShort = base::TimeDelta::FromSeconds(3);

// The insets from the bubble border to the text inside.
constexpr gfx::Insets kBubbleContentsInsets(12, 16);

}  // namespace

FeaturePromoBubbleView::FeaturePromoBubbleView(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ActivationAction activation_action,
    int string_specifier,
    base::Optional<int> screenreader_string_specifier,
    base::Optional<ui::Accelerator> feature_accelerator,
    std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout)
    : BubbleDialogDelegateView(anchor_view, arrow),
      activation_action_(activation_action),
      feature_promo_bubble_timeout_(std::move(feature_promo_bubble_timeout)) {
  DCHECK(anchor_view);
  UseCompactMargins();

  // If the timeout was not explicitly specified, use the default values.
  if (!feature_promo_bubble_timeout_) {
    feature_promo_bubble_timeout_ =
        std::make_unique<FeaturePromoBubbleTimeout>(kDelayDefault, kDelayShort);
  }

  const base::string16 body_text = l10n_util::GetStringUTF16(string_specifier);

  // Feature promos are purely informational. We can skip reading the UI
  // elements inside the bubble and just have the information announced when the
  // bubble shows. To do so, we change the a11y tree to make this a leaf node
  // and set the name to the message we want to announce.
  GetViewAccessibility().OverrideIsLeaf(true);
  if (!screenreader_string_specifier) {
    accessible_name_ = body_text;
  } else if (feature_accelerator) {
    accessible_name_ = l10n_util::GetStringFUTF16(
        *screenreader_string_specifier, feature_accelerator->GetShortcutText());
  } else {
    accessible_name_ =
        l10n_util::GetStringUTF16(*screenreader_string_specifier);
  }

  // We get the theme provider from the anchor view since our widget hasn't been
  // created yet.
  const ui::ThemeProvider* theme_provider = anchor_view->GetThemeProvider();
  DCHECK(theme_provider);

  const SkColor background_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_BACKGROUND);
  const SkColor text_color = theme_provider->GetColor(
      ThemeProperties::COLOR_FEATURE_PROMO_BUBBLE_TEXT);

  auto box_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kBubbleContentsInsets, 0);
  box_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);
  box_layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
  SetLayoutManager(std::move(box_layout));

  auto* label = new views::Label(body_text);
  label->SetBackgroundColor(background_color);
  label->SetEnabledColor(text_color);
  AddChildView(label);

  if (activation_action == ActivationAction::DO_NOT_ACTIVATE) {
    SetCanActivate(activation_action == ActivationAction::ACTIVATE);
    set_shadow(views::BubbleBorder::BIG_SHADOW);
  }

  set_margins(gfx::Insets());
  set_title_margins(gfx::Insets());
  DialogDelegate::set_buttons(ui::DIALOG_BUTTON_NONE);

  set_color(background_color);

  views::Widget* widget = views::BubbleDialogDelegateView::CreateBubble(this);

  GetBubbleFrameView()->SetCornerRadius(
      ChromeLayoutProvider::Get()->GetCornerRadiusMetric(views::EMPHASIS_HIGH));

  widget->Show();
  feature_promo_bubble_timeout_->OnBubbleShown(this);
}

FeaturePromoBubbleView::~FeaturePromoBubbleView() = default;

// static
FeaturePromoBubbleView* FeaturePromoBubbleView::CreateOwned(
    views::View* anchor_view,
    views::BubbleBorder::Arrow arrow,
    ActivationAction activation_action,
    int string_specifier,
    base::Optional<int> screenreader_string_specifier,
    base::Optional<ui::Accelerator> feature_accelerator,
    std::unique_ptr<FeaturePromoBubbleTimeout> feature_promo_bubble_timeout) {
  return new FeaturePromoBubbleView(
      anchor_view, arrow, activation_action, string_specifier,
      screenreader_string_specifier, feature_accelerator,
      std::move(feature_promo_bubble_timeout));
}

void FeaturePromoBubbleView::CloseBubble() {
  GetWidget()->Close();
}

bool FeaturePromoBubbleView::OnMousePressed(const ui::MouseEvent& event) {
  base::RecordAction(
      base::UserMetricsAction("InProductHelp.Promos.BubbleClicked"));
  return false;
}

void FeaturePromoBubbleView::OnMouseEntered(const ui::MouseEvent& event) {
  feature_promo_bubble_timeout_->OnMouseEntered();
}

void FeaturePromoBubbleView::OnMouseExited(const ui::MouseEvent& event) {
  feature_promo_bubble_timeout_->OnMouseExited();
}

gfx::Rect FeaturePromoBubbleView::GetBubbleBounds() {
  gfx::Rect bounds = BubbleDialogDelegateView::GetBubbleBounds();
  if (activation_action_ == ActivationAction::DO_NOT_ACTIVATE) {
    if (base::i18n::IsRTL())
      bounds.Offset(5, 0);
    else
      bounds.Offset(-5, 0);
  }
  return bounds;
}

ax::mojom::Role FeaturePromoBubbleView::GetAccessibleWindowRole() {
  // Since we don't have any controls for the user to interact with (we're just
  // an information bubble), override our role to kAlert.
  return ax::mojom::Role::kAlert;
}

base::string16 FeaturePromoBubbleView::GetAccessibleWindowTitle() const {
  return accessible_name_;
}
