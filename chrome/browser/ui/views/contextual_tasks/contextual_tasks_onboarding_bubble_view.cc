// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_onboarding_bubble_view.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider_key.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/range/range.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/style/typography.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace contextual_tasks {

namespace {

// Forced light-mode colors per UX spec to match the light-themed composebox.
constexpr SkColor kBubbleBgColor = SK_ColorWHITE;

}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksOnboardingBubbleView,
                                      kBubbleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksOnboardingBubbleView,
                                      kTitleElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksOnboardingBubbleView,
                                      kBodyElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksOnboardingBubbleView,
                                      kAcceptButtonElementId);

ContextualTasksOnboardingBubbleView::ContextualTasksOnboardingBubbleView(
    views::BubbleAnchor anchor,
    Variant variant,
    base::OnceClosure on_accept_callback,
    base::RepeatingClosure on_learn_more_callback)
    : BubbleDialogDelegate(anchor,
                           views::BubbleBorder::Arrow::BOTTOM_LEFT,
                           views::BubbleBorder::STANDARD_SHADOW),
      variant_(variant),
      on_accept_callback_(std::move(on_accept_callback)),
      on_learn_more_callback_(std::move(on_learn_more_callback)) {
  static constexpr SkColor kBubbleTextColor = SK_ColorBLACK;
  static constexpr SkColor kBubbleAccentBlue = SkColorSetRGB(0x0B, 0x57, 0xD0);
  const bool is_onboarding = variant_ == Variant::kOnboarding;
  SetBackgroundColor(kBubbleBgColor);
  set_corner_radius(8);
  set_fixed_width(288);
  set_margins(gfx::Insets(16));
  set_close_on_deactivate(false);
  set_adjust_if_offscreen(false);
  SetShowTitle(false);
  SetShowCloseButton(false);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kNone));
  if (ui::TrackedElement* tracked_element = anchor.GetIfElement()) {
    // Safe because `this` owns `anchor_element_hidden_subscription_`.
    anchor_element_hidden_subscription_ =
        ui::ElementTracker::GetElementTracker()->AddElementHiddenCallback(
            tracked_element->identifier(), tracked_element->context(),
            base::BindRepeating(
                &ContextualTasksOnboardingBubbleView::OnAnchorElementHidden,
                base::Unretained(this), tracked_element));
  }

  SetProperty(views::kElementIdentifierKey, kBubbleElementId);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      /*between_child_spacing=*/4));

  auto title = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(
          is_onboarding
              ? IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_TITLE
              : IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_SHORT_TITLE),
      views::style::CONTEXT_LABEL, views::style::STYLE_BODY_4_MEDIUM);
  title->SetProperty(views::kElementIdentifierKey, kTitleElementId);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetMultiLine(true);
  title->SetLineHeight(16);
  title->SetAutoColorReadabilityEnabled(false);
  title->SetEnabledColor(kBubbleTextColor);
  title_label_ = AddChildView(std::move(title));

  // Reuses pre-existing frozen WebUI strings without adding `.grd`
  // placeholders.
  std::u16string description = l10n_util::GetStringUTF16(
      is_onboarding ? IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION
                    : IDS_LENS_COBROWSE_CURRENT_TAB_IPH_DESCRIPTION);
  std::u16string learn_more = l10n_util::GetStringUTF16(
      is_onboarding ? IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_LEARN_MORE
                    : IDS_LEARN_MORE);
  std::u16string full_text = base::StrCat({description, u" ", learn_more});

  auto body = std::make_unique<views::StyledLabel>();
  body->SetProperty(views::kElementIdentifierKey, kBodyElementId);
  body->SetText(full_text);
  body->SetTextContext(views::style::CONTEXT_LABEL);
  body->SetDefaultTextStyle(views::style::STYLE_BODY_4);
  body->SetLineHeight(16);
  body->SetDisplayedOnBackgroundColor(kBubbleBgColor);
  body->SetAutoColorReadabilityEnabled(false);

  views::StyledLabel::RangeStyleInfo body_style;
  body_style.text_style = views::style::STYLE_BODY_4;
  body_style.override_color = kBubbleTextColor;
  body->AddStyleRange(gfx::Range(0, description.length() + 1), body_style);

  views::StyledLabel::RangeStyleInfo link_style =
      views::StyledLabel::RangeStyleInfo::CreateForLink(
          on_learn_more_callback_);
  link_style.text_style = views::style::STYLE_LINK_4;
  link_style.override_color = kBubbleAccentBlue;
  body->AddStyleRange(gfx::Range(description.length() + 1, full_text.length()),
                      link_style);
  body_label_ = AddChildView(std::move(body));

  auto* button_container =
      AddChildView(std::make_unique<views::BoxLayoutView>());
  button_container->SetProperty(views::kMarginsKey,
                                gfx::Insets::TLBR(8, 0, 0, 0));
  button_container->SetMainAxisAlignment(views::LayoutAlignment::kEnd);

  // Safe because `accept_button_` is a child view owned by `this`.
  auto accept_button = std::make_unique<views::MdTextButton>(
      base::BindRepeating(
          &ContextualTasksOnboardingBubbleView::OnAcceptButtonPressed,
          base::Unretained(this)),
      l10n_util::GetStringUTF16(
          IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_ACCEPT_BUTTON));
  accept_button->SetProperty(views::kElementIdentifierKey,
                             kAcceptButtonElementId);
  accept_button->SetStyle(ui::ButtonStyle::kProminent);
  accept_button->SetLabelStyle(views::style::STYLE_BODY_4_MEDIUM);
  accept_button->SetBgColorIdOverride(ui::kColorRefPrimary40);
  accept_button->SetEnabledTextColors(kBubbleBgColor);
  accept_button->SetCornerRadius(100);
  accept_button->SetMinSize(gfx::Size(62, 32));
  accept_button->SetCustomPadding(gfx::Insets::VH(6, 16));
  accept_button_ = button_container->AddChildView(std::move(accept_button));
}

ContextualTasksOnboardingBubbleView::~ContextualTasksOnboardingBubbleView() {
  SetAnchor(views::BubbleAnchor());
}

// static
std::unique_ptr<views::Widget>
ContextualTasksOnboardingBubbleView::CreateAndShow(
    views::BubbleAnchor anchor,
    Variant variant,
    base::OnceClosure on_accept_callback,
    base::RepeatingClosure on_learn_more_callback,
    gfx::NativeView parent_window) {
  auto bubble_view = std::make_unique<ContextualTasksOnboardingBubbleView>(
      anchor, variant, std::move(on_accept_callback),
      std::move(on_learn_more_callback));
  if (parent_window) {
    bubble_view->set_parent_window(parent_window);
  }

  // `DialogClientView` takes ownership of `bubble_view` as its child contents
  // view during `CreateBubble`.
  std::unique_ptr<views::Widget> widget =
      views::BubbleDialogDelegate::CreateBubble(
          std::move(bubble_view).release());
  widget->SetColorModeOverride(ui::ColorProviderKey::ColorMode::kLight);

  if (auto* frame_view = static_cast<views::BubbleFrameView*>(
          widget->non_client_view()->frame_view())) {
    frame_view->SetDisplayVisibleArrow(true);
    if (frame_view->bubble_border()) {
      frame_view->bubble_border()->set_draw_border_stroke(false);
    }
  }
  if (auto* delegate = widget->widget_delegate()->AsBubbleDialogDelegate()) {
    delegate->SizeToContents();
  }
  widget->ShowInactive();
  return widget;
}

views::View* ContextualTasksOnboardingBubbleView::GetContentsView() {
  return this;
}

views::Widget* ContextualTasksOnboardingBubbleView::GetWidget() {
  return View::GetWidget();
}

const views::Widget* ContextualTasksOnboardingBubbleView::GetWidget() const {
  return View::GetWidget();
}

void ContextualTasksOnboardingBubbleView::OnAcceptButtonPressed() {
  base::WeakPtr<views::Widget> widget =
      GetWidget() ? GetWidget()->GetWeakPtr() : nullptr;
  if (on_accept_callback_) {
    std::move(on_accept_callback_).Run();
  }
  if (widget && !widget->IsClosed()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kAcceptButtonClicked);
  }
}

void ContextualTasksOnboardingBubbleView::OnAnchorElementHidden(
    ui::TrackedElement* tracked_element,
    ui::TrackedElement* hidden_element) {
  if (hidden_element != tracked_element) {
    return;
  }
  SetAnchor(views::BubbleAnchor());
  anchor_element_hidden_subscription_ = base::CallbackListSubscription();
  if (views::Widget* widget = GetWidget(); widget && !widget->IsClosed()) {
    widget->Close();
  }
}

BEGIN_METADATA(ContextualTasksOnboardingBubbleView)
END_METADATA

}  // namespace contextual_tasks
