// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_thread_item_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/views/tabs/projects/layout_constants.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_utils.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"

namespace {
inline constexpr int kChatTypeIconSize = 16;
inline constexpr gfx::Insets kChatTypeIconMargins = gfx::Insets(4);

static bool disable_animations_for_testing_ = false;
}  // namespace

ProjectsPanelThreadItemView::ProjectsPanelThreadItemView(
    const contextual_tasks::Thread& thread,
    ThreadPressedCallback pressed_callback)
    : chat_type_icon_(projects_panel::GetIconForThreadType(thread.type)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetInteriorMargin(projects_panel::kListItemMargins)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  auto& accessibility = GetViewAccessibility();
  auto thread_title = base::UTF8ToUTF16(thread.title);
  switch (thread.type) {
    case contextual_tasks::ThreadType::kAiMode:
      accessibility.SetName(l10n_util::GetStringFUTF16(
          IDS_OPEN_AI_MODE_THREAD_ACCESSIBILITY_LABEL, thread_title));
      SetTooltipText(
          l10n_util::GetStringUTF16(IDS_OPEN_AI_MODE_THREAD_TOOLTIP));
      break;
    case contextual_tasks::ThreadType::kGemini:
      accessibility.SetName(l10n_util::GetStringFUTF16(
          IDS_OPEN_GEMINI_THREAD_ACCESSIBILITY_LABEL, thread_title));
      SetTooltipText(l10n_util::GetStringUTF16(IDS_OPEN_GEMINI_THREAD_TOOLTIP));
      break;
    case contextual_tasks::ThreadType::kUnknown:
      NOTREACHED();
  }

  projects_panel::ConfigureInkDropForButton(this);

  auto chat_type_image_model_ = ui::ImageModel::FromVectorIcon(
      *chat_type_icon_, kColorProjectsPanelButtonIcon, kChatTypeIconSize);
  auto* chat_type_image_ =
      AddChildView(std::make_unique<views::ImageView>(chat_type_image_model_));
  chat_type_image_->SetCanProcessEventsWithinSubtree(false);
  chat_type_image_->SetProperty(views::kMarginsKey, kChatTypeIconMargins);

  title_ = AddChildView(std::make_unique<views::Label>());
  title_->SetText(base::UTF8ToUTF16(thread.title));
  title_->SetTextStyle(views::style::TextStyle::STYLE_BODY_3);
  title_->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_TO_HEAD);
  title_->SetElideBehavior(gfx::FADE_TAIL);
  title_->SetProperty(views::kMarginsKey,
                      projects_panel::kListItemTitleMargins);
  title_->SetBackgroundColor(SK_ColorTRANSPARENT);
  // Prevent the title from showing its own tooltip since we've set one for the
  // entire view.
  title_->SetHandlesTooltips(false);
  title_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));

  trailing_icon_ = AddChildView(std::make_unique<views::ImageView>());
  trailing_icon_->SetCanProcessEventsWithinSubtree(false);
  trailing_icon_->SetProperty(views::kMarginsKey,
                              projects_panel::kTrailingIconMargins);
  ui::ImageModel open_in_new_image_model = ui::ImageModel::FromVectorIcon(
      kOpenInNewIcon, kColorProjectsPanelButtonDisabledIcon,
      projects_panel::kTrailingIconSize);
  trailing_icon_->SetImage(open_in_new_image_model);

  // Paint the trailing icon to a layer so we can adjust its opacity during the
  // hover animation.
  trailing_icon_->SetPaintToLayer();
  trailing_icon_->layer()->SetFillsBoundsOpaquely(false);
  trailing_icon_->layer()->SetOpacity(0.0f);
  trailing_icon_->SetVisible(false);

  trailing_icon_fade_animation_.SetSlideDuration(
      projects_panel::kListItemHoverFadeAnimationDuration);

  SetCallback(base::BindRepeating(std::move(pressed_callback), thread.server_id,
                                  thread.type));

  SetProperty(views::kElementIdentifierKey,
              kProjectsPanelThreadListItemViewElementId);
}

ProjectsPanelThreadItemView::~ProjectsPanelThreadItemView() = default;

void ProjectsPanelThreadItemView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateHoverState();
}

void ProjectsPanelThreadItemView::OnMouseExited(const ui::MouseEvent& event) {
#if BUILDFLAG(IS_LINUX)
  // Bypasses the synchronous IsMouseHovered() check which can be stale on Linux
  // Wayland/X11 due to asynchronous cursor updates during mouse exit events.
  UpdateHoverStateForced(/*is_hovered=*/false);
#else
  UpdateHoverState();
#endif
}

void ProjectsPanelThreadItemView::OnMouseMoved(const ui::MouseEvent& event) {
  // Mouse enter and exit events are flaky on Linux, so this ensures the hover
  // state is still applied.
  UpdateHoverState();
}

void ProjectsPanelThreadItemView::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation != &trailing_icon_fade_animation_) {
    views::Button::AnimationProgressed(animation);
    return;
  }

  const float value = static_cast<float>(animation->GetCurrentValue());
  trailing_icon_->SetVisible(value > 0.0f);
  trailing_icon_->layer()->SetOpacity(value);
}

// static
void ProjectsPanelThreadItemView::disable_animations_for_testing() {
  disable_animations_for_testing_ = true;
}

void ProjectsPanelThreadItemView::UpdateHoverState() {
  UpdateHoverStateForced(IsMouseHovered());
}

void ProjectsPanelThreadItemView::UpdateHoverStateForced(bool is_hovered) {
  if (disable_animations_for_testing_) {
    trailing_icon_->SetVisible(is_hovered);
    return;
  }

  if (is_hovered) {
    trailing_icon_fade_animation_.Show();
  } else {
    trailing_icon_fade_animation_.Hide();
  }
}

BEGIN_METADATA(ProjectsPanelThreadItemView)
END_METADATA
