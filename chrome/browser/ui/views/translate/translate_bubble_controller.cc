// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"

#include <memory>

#include "chrome/browser/ui/translate/partial_translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/translate_bubble_ui_action_logger.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/web_contents.h"

TranslateBubbleController::~TranslateBubbleController() = default;

// static
TranslateBubbleController* TranslateBubbleController::GetOrCreate(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  TranslateBubbleController::CreateForWebContents(web_contents);
  return TranslateBubbleController::FromWebContents(web_contents);
}

views::Widget* TranslateBubbleController::ShowTranslateBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    std::unique_ptr<translate::TranslateUIDelegate> ui_delegate,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    LocationBarBubbleDelegateView::DisplayReason reason) {
  // If the partial translate bubble is already being shown, close it before
  // showing the full translate bubble.
  if (partial_translate_bubble_view_)
    partial_translate_bubble_view_->CloseBubble();

  if (translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
        translate_bubble_view_->model()->GetViewState() ==
            TranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
      return nullptr;
    }
    translate_bubble_view_->SetViewState(step, error_type);
    return nullptr;
  }

  if (step == translate::TRANSLATE_STEP_AFTER_TRANSLATE &&
      reason == LocationBarBubbleDelegateView::AUTOMATIC) {
    return nullptr;
  }

  content::WebContents* web_contents = &GetWebContents();
  std::unique_ptr<TranslateBubbleModel> model =
      std::make_unique<TranslateBubbleModelImpl>(step, std::move(ui_delegate));

  // TODO(cuianthony): In the follow-up CL, the TranslateBubbleView constructor
  // will take the OnTranslateBubbleClosed Closure so that it can notify this
  // controller when it is closing.
  auto translate_bubble_view = std::make_unique<TranslateBubbleView>(
      anchor_view, std::move(model), error_type, web_contents);
  translate_bubble_view_ = translate_bubble_view.get();

  if (highlighted_button)
    translate_bubble_view_->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(translate_bubble_view));

  // TAB UI has the same view throughout. Select the right tab based on |step|
  // upon initialization.
  translate_bubble_view_->SetViewState(step, error_type);

  translate_bubble_view_->ShowForReason(reason);
  translate::ReportUiAction(translate::BUBBLE_SHOWN);

  ChromeTranslateClient::GetManagerFromWebContents(web_contents)
      ->GetActiveTranslateMetricsLogger()
      ->LogUIChange(true);

  return bubble_widget;
}

views::Widget* TranslateBubbleController::ShowPartialTranslateBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    PartialTranslateBubbleModel::ViewState view_state,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors::Type error_type) {
  // If the other translate bubble is already being shown, close it before
  // showing this one.
  if (translate_bubble_view_)
    translate_bubble_view_->CloseBubble();

  if (partial_translate_bubble_view_) {
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (partial_translate_bubble_view_->model()->GetViewState() ==
            PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
        partial_translate_bubble_view_->model()->GetViewState() ==
            PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
      return nullptr;
    }
    partial_translate_bubble_view_->SetViewState(view_state, error_type);
    return nullptr;
  }
  content::WebContents* web_contents = &GetWebContents();
  // TODO(crbug/1314825): When the PartialTranslateManager is added it
  // will replace and take the role of the TranslateUIDelegate.
  auto ui_delegate = std::make_unique<translate::TranslateUIDelegate>(
      ChromeTranslateClient::GetManagerFromWebContents(web_contents)
          ->GetWeakPtr(),
      source_language, target_language);

  // TODO(cuianthony): In the follow-up CL, the TranslateBubbleView constructor
  // will take the OnPartialTranslateBubbleClosed Closure so that it can notify
  // this controller when it is closing.
  auto model = std::make_unique<PartialTranslateBubbleModelImpl>(
      view_state, std::move(ui_delegate));
  auto partial_translate_bubble_view =
      std::make_unique<PartialTranslateBubbleView>(
          anchor_view, std::move(model), error_type, web_contents);
  partial_translate_bubble_view_ = partial_translate_bubble_view.get();

  if (highlighted_button)
    partial_translate_bubble_view_->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      partial_translate_bubble_view_.get());

  partial_translate_bubble_view_->SetViewState(view_state, error_type);

  partial_translate_bubble_view_->ShowForReason(
      LocationBarBubbleDelegateView::USER_GESTURE);

  return bubble_widget;
}

void TranslateBubbleController::CloseBubble() {
  if (translate_bubble_view_) {
    translate_bubble_view_->CloseBubble();
  } else if (partial_translate_bubble_view_) {
    partial_translate_bubble_view_->CloseBubble();
  }
}

TranslateBubbleView* TranslateBubbleController::GetTranslateBubble() const {
  return translate_bubble_view_;
}

PartialTranslateBubbleView*
TranslateBubbleController::GetPartialTranslateBubble() const {
  return partial_translate_bubble_view_;
}

base::OnceClosure
TranslateBubbleController::GetOnTranslateBubbleClosedCallback() {
  return base::BindOnce(&TranslateBubbleController::OnTranslateBubbleClosed,
                        weak_ptr_factory_.GetWeakPtr());
}

base::OnceClosure
TranslateBubbleController::GetOnPartialTranslateBubbleClosedCallback() {
  return base::BindOnce(
      &TranslateBubbleController::OnPartialTranslateBubbleClosed,
      weak_ptr_factory_.GetWeakPtr());
}

void TranslateBubbleController::OnTranslateBubbleClosed() {
  translate_bubble_view_ = nullptr;
}

void TranslateBubbleController::OnPartialTranslateBubbleClosed() {
  partial_translate_bubble_view_ = nullptr;
}

TranslateBubbleController::TranslateBubbleController(
    content::WebContents* web_contents)
    : content::WebContentsUserData<TranslateBubbleController>(*web_contents) {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(TranslateBubbleController);
