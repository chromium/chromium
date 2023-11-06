// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/translate/translate_bubble_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_model_impl.h"
#include "chrome/browser/ui/translate/partial_translate_bubble_ui_action_logger.h"
#include "chrome/browser/ui/translate/translate_bubble_model_impl.h"
#include "chrome/browser/ui/views/translate/partial_translate_bubble_view.h"
#include "components/contextual_search/core/browser/contextual_search_delegate_impl.h"
#include "components/translate/content/browser/partial_translate_manager.h"
#include "components/translate/core/browser/translate_language_list.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_ui_delegate.h"
#include "components/translate/core/browser/translate_ui_languages_manager.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"

namespace {

const char kTranslatePartialTranslationSelectionCharacterCount[] =
    "Translate.PartialTranslation.Selection.CharacterCount";

}

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
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    LocationBarBubbleDelegateView::DisplayReason reason) {
  // If the Partial Translate bubble is already being shown, close it before
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

  std::unique_ptr<TranslateBubbleModel> model;
  if (model_factory_callback_) {
    model = model_factory_callback_.Run();
  } else {
    auto ui_delegate = std::make_unique<translate::TranslateUIDelegate>(
        ChromeTranslateClient::GetManagerFromWebContents(web_contents)
            ->GetWeakPtr(),
        source_language, target_language);
    model = std::make_unique<TranslateBubbleModelImpl>(step,
                                                       std::move(ui_delegate));
  }

  auto translate_bubble_view = std::make_unique<TranslateBubbleView>(
      anchor_view, std::move(model), error_type, web_contents,
      GetOnTranslateBubbleClosedCallback());
  translate_bubble_view_ = translate_bubble_view.get();

  if (highlighted_button)
    translate_bubble_view_->SetHighlightedButton(highlighted_button);
  views::Widget* bubble_widget = views::BubbleDialogDelegateView::CreateBubble(
      std::move(translate_bubble_view));

  // TAB UI has the same view throughout. Select the right tab based on |step|
  // upon initialization.
  translate_bubble_view_->SetViewState(step, error_type);

  translate_bubble_view_->ShowForReason(reason);

  translate_bubble_view_->model()->ReportUIChange(true);

  return bubble_widget;
}

void TranslateBubbleController::StartPartialTranslate(
    views::View* anchor_view,
    views::Button* highlighted_button,
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& text_selection) {
  CreatePartialTranslateBubble(anchor_view, highlighted_button,
                               PartialTranslateBubbleModel::VIEW_STATE_WAITING,
                               source_language, target_language, text_selection,
                               /*target_text=*/u"",
                               translate::TranslateErrors::NONE);
  // If response time is <=kDesktopPartialTranslateBubbleShowDelayMs, the bubble
  // will be shown directly with the translation. If response time is longer,
  // the bubble will be shown in a loading state until the translation is ready.
  partial_translate_timer_.Start(
      FROM_HERE,
      base::Milliseconds(translate::kDesktopPartialTranslateBubbleShowDelayMs),
      base::BindOnce(&TranslateBubbleController::OnPartialTranslateWaitExpired,
                     weak_ptr_factory_.GetWeakPtr()));

  partial_translate_bubble_view_->model()->Translate(&GetWebContents());
}

void TranslateBubbleController::OnPartialTranslateWaitExpired() {
  if (!partial_translate_bubble_view_) {
    return;
  }

  if (partial_translate_bubble_view_->GetViewState() ==
      PartialTranslateBubbleModel::VIEW_STATE_WAITING) {
    partial_translate_bubble_view_->ShowForReason(
        LocationBarBubbleDelegateView::USER_GESTURE);
    translate::ReportPartialTranslateBubbleUiAction(
        translate::PartialTranslateBubbleUiEvent::BUBBLE_SHOWN);
  }
}

void TranslateBubbleController::OnPartialTranslateComplete() {
  // Stop the wait timer so we don't revert back to the waiting view.
  partial_translate_timer_.Stop();

  if (!partial_translate_bubble_view_) {
    return;
  }

  if (partial_translate_bubble_view_->model()->GetError() !=
      translate::TranslateErrors::NONE) {
    partial_translate_bubble_view_->SetViewState(
        PartialTranslateBubbleModel::VIEW_STATE_ERROR,
        partial_translate_bubble_view_->model()->GetError());
  } else {
    partial_translate_bubble_view_->SetViewState(
        PartialTranslateBubbleModel::VIEW_STATE_AFTER_TRANSLATE,
        translate::TranslateErrors::NONE);
  }

  partial_translate_bubble_view_->MaybeUpdateSourceLanguageCombobox();
  partial_translate_bubble_view_->ShowForReason(
      LocationBarBubbleDelegateView::USER_GESTURE);
  translate::ReportPartialTranslateBubbleUiAction(
      translate::PartialTranslateBubbleUiEvent::BUBBLE_SHOWN);
}

void TranslateBubbleController::CreatePartialTranslateBubble(
    views::View* anchor_view,
    views::Button* highlighted_button,
    PartialTranslateBubbleModel::ViewState view_state,
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& source_text,
    const std::u16string& target_text,
    translate::TranslateErrors error_type) {
  // If the other Translate bubble is already being shown, close it before
  // showing this one.
  if (translate_bubble_view_)
    translate_bubble_view_->CloseBubble();

  // Truncate text selection, if needed. Log the length of the text selection
  // before truncating.
  base::UmaHistogramCounts100000(
      kTranslatePartialTranslationSelectionCharacterCount,
      source_text.length());
  std::u16string truncated_source_text = gfx::TruncateString(
      source_text,
      translate::kDesktopPartialTranslateTextSelectionMaxCharacters,
      gfx::WORD_BREAK);
  bool is_truncated = (source_text.compare(truncated_source_text) != 0);

  if (partial_translate_bubble_view_) {
    PartialTranslateBubbleModel* model =
        partial_translate_bubble_view_->model();
    model->SetSourceLanguage(source_language);
    model->SetTargetLanguage(target_language);
    model->SetSourceText(truncated_source_text);
    model->SetSourceTextTruncated(is_truncated);
    model->SetTargetText(target_text);
    // When the user reads the advanced setting panel, the bubble should not be
    // changed because they are focusing on the bubble.
    if (partial_translate_bubble_view_->model()->GetViewState() ==
            PartialTranslateBubbleModel::VIEW_STATE_SOURCE_LANGUAGE ||
        partial_translate_bubble_view_->model()->GetViewState() ==
            PartialTranslateBubbleModel::VIEW_STATE_TARGET_LANGUAGE) {
      return;
    }
    partial_translate_bubble_view_->SetViewState(view_state, error_type);
    return;
  }
  content::WebContents* web_contents = &GetWebContents();

  std::unique_ptr<PartialTranslateBubbleModel> model;
  if (partial_model_factory_callback_) {
    model = partial_model_factory_callback_.Run();
  } else {
    std::vector<std::string> language_codes;
    translate::TranslateLanguageList::GetSupportedPartialTranslateLanguages(
        &language_codes);
    auto translate_ui_languages_manager =
        std::make_unique<translate::TranslateUILanguagesManager>(
            ChromeTranslateClient::GetManagerFromWebContents(web_contents)
                ->GetWeakPtr(),
            language_codes, source_language, target_language);

    Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    auto partial_translate_manager = std::make_unique<PartialTranslateManager>(
        std::make_unique<ContextualSearchDelegateImpl>(
            profile->GetURLLoaderFactory(),
            TemplateURLServiceFactory::GetForProfile(profile)));

    model = std::make_unique<PartialTranslateBubbleModelImpl>(
        view_state, error_type, truncated_source_text, target_text,
        std::move(partial_translate_manager),
        std::move(translate_ui_languages_manager));
  }
  model->SetSourceTextTruncated(is_truncated);

  model->AddObserver(this);

  auto partial_translate_bubble_view =
      std::make_unique<PartialTranslateBubbleView>(
          anchor_view, std::move(model), web_contents,
          GetOnPartialTranslateBubbleClosedCallback());
  partial_translate_bubble_view_ = partial_translate_bubble_view.get();
  if (highlighted_button)
    partial_translate_bubble_view_->SetHighlightedButton(highlighted_button);
  views::BubbleDialogDelegateView::CreateBubble(
      std::move(partial_translate_bubble_view));
  partial_translate_bubble_view_->SetViewState(view_state, error_type);
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

void TranslateBubbleController::SetTranslateBubbleModelFactory(
    base::RepeatingCallback<std::unique_ptr<TranslateBubbleModel>()> callback) {
  model_factory_callback_ = std::move(callback);
}

void TranslateBubbleController::SetPartialTranslateBubbleModelFactory(
    base::RepeatingCallback<std::unique_ptr<PartialTranslateBubbleModel>()>
        callback) {
  partial_model_factory_callback_ = std::move(callback);
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
