// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_BUBBLE_VIEW_H_

#include <memory>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/bubble/bubble_anchor.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace ui {
class TrackedElement;
}  // namespace ui

namespace views {
class Label;
class MdTextButton;
class StyledLabel;
class Widget;
}  // namespace views

namespace contextual_tasks {

// Views bubble that informs the user about tab sharing and privacy.
class ContextualTasksOnboardingBubbleView : public views::BubbleDialogDelegate,
                                            public views::View {
  METADATA_HEADER(ContextualTasksOnboardingBubbleView, views::View)

 public:
  enum class Variant { kOnboarding, kAskGoogle };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBubbleElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTitleElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kBodyElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAcceptButtonElementId);

  ContextualTasksOnboardingBubbleView(
      views::BubbleAnchor anchor,
      Variant variant,
      base::OnceClosure on_accept_callback,
      base::RepeatingClosure on_learn_more_callback);
  ContextualTasksOnboardingBubbleView(
      const ContextualTasksOnboardingBubbleView&) = delete;
  ContextualTasksOnboardingBubbleView& operator=(
      const ContextualTasksOnboardingBubbleView&) = delete;
  ~ContextualTasksOnboardingBubbleView() override;

  static std::unique_ptr<views::Widget> CreateAndShow(
      views::BubbleAnchor anchor,
      Variant variant,
      base::OnceClosure on_accept_callback,
      base::RepeatingClosure on_learn_more_callback,
      gfx::NativeView parent_window = gfx::NativeView());

  // views::BubbleDialogDelegate:
  views::View* GetContentsView() override;

  // views::View:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  Variant variant_for_testing() const { return variant_; }
  views::Label* title_label_for_testing() { return title_label_; }
  views::StyledLabel* body_label_for_testing() { return body_label_; }
  views::MdTextButton* accept_button_for_testing() { return accept_button_; }

 private:
  void OnAcceptButtonPressed();
  void OnAnchorElementHidden(ui::TrackedElement* tracked_element,
                             ui::TrackedElement* hidden_element);

  const Variant variant_;
  base::OnceClosure on_accept_callback_;
  base::RepeatingClosure on_learn_more_callback_;

  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::StyledLabel> body_label_ = nullptr;
  raw_ptr<views::MdTextButton> accept_button_ = nullptr;
  base::CallbackListSubscription anchor_element_hidden_subscription_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_UI_VIEWS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_ONBOARDING_BUBBLE_VIEW_H_
