// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "chrome/browser/accessibility/live_caption_controller.h"
#include "chrome/browser/accessibility/live_caption_controller_factory.h"
#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/live_caption/views/caption_bubble.h"
#include "components/live_caption/views/caption_bubble_model.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create() {
  return std::make_unique<CaptionBubbleControllerViews>();
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews() {
  caption_bubble_ = new CaptionBubble(
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)),
      /* hide_on_inactivity= */ true);
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;
}

bool CaptionBubbleControllerViews::OnTranscription(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host,
    const media::SpeechRecognitionResult& result) {
  if (!caption_bubble_)
    return false;
  SetActiveModel(live_caption_speech_recognition_host);
  if (active_model_->IsClosed())
    return false;

  // If the caption bubble has no activity and it receives a final
  // transcription, don't set text. The speech service sends a final
  // transcription after several seconds of no audio. This prevents the bubble
  // reappearing with a final transcription after it had disappeared due to no
  // activity.
  if (!caption_bubble_->HasActivity() && result.is_final)
    return true;

  active_model_->SetPartialText(result.transcription);
  if (result.is_final)
    active_model_->CommitPartialText();

  return true;
}

void CaptionBubbleControllerViews::OnError(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host) {
  if (!caption_bubble_)
    return;
  SetActiveModel(live_caption_speech_recognition_host);
  if (active_model_->IsClosed())
    return;
  active_model_->OnError();
}

void CaptionBubbleControllerViews::OnAudioStreamEnd(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host) {
  if (!caption_bubble_)
    return;

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[live_caption_speech_recognition_host].get();
  if (active_model_ == caption_bubble_model) {
    active_model_ = nullptr;
    caption_bubble_->SetModel(nullptr);
  }
  caption_bubble_models_.erase(live_caption_speech_recognition_host);
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    absl::optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

void CaptionBubbleControllerViews::SetActiveModel(
    LiveCaptionSpeechRecognitionHost* live_caption_speech_recognition_host) {
  if (!caption_bubble_models_.count(live_caption_speech_recognition_host)) {
    content::WebContents* web_contents =
        live_caption_speech_recognition_host->GetWebContents();
    views::Widget* context_widget =
        web_contents ? views::Widget::GetTopLevelWidgetForNativeView(
                           web_contents->GetNativeView())
                     : nullptr;
    absl::optional<gfx::Rect> context_bounds = absl::nullopt;
    if (context_widget)
      context_bounds = context_widget->GetClientAreaBoundsInScreen();
    caption_bubble_models_.emplace(
        live_caption_speech_recognition_host,
        std::make_unique<CaptionBubbleModel>(
            context_bounds,
            base::BindRepeating(&CaptionBubbleControllerViews::ActivateContext,
                                base::Unretained(this), web_contents)));
  }

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[live_caption_speech_recognition_host].get();
  if (active_model_ != caption_bubble_model) {
    active_model_ = caption_bubble_model;
    caption_bubble_->SetModel(active_model_);
  }
}

void CaptionBubbleControllerViews::ActivateContext(
    content::WebContents* web_contents) {
  if (!web_contents)
    return;
  // Activate the web contents and the browser window that the web contents is
  // in. Order matters: web contents needs to be active in order for the widget
  // getter to work.
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (!browser)
    return;
  TabStripModel* tab_strip_model = browser->tab_strip_model();
  if (!tab_strip_model)
    return;
  int index = tab_strip_model->GetIndexOfWebContents(web_contents);
  if (index == TabStripModel::kNoTab)
    return;
  tab_strip_model->ActivateTabAt(index);
  views::Widget* context_widget = views::Widget::GetTopLevelWidgetForNativeView(
      web_contents->GetNativeView());
  if (context_widget)
    context_widget->Activate();
}

bool CaptionBubbleControllerViews::IsWidgetVisibleForTesting() {
  return caption_widget_ && caption_widget_->IsVisible();
}

std::string CaptionBubbleControllerViews::GetBubbleLabelTextForTesting() {
  return caption_bubble_
             ? base::UTF16ToUTF8(
                   caption_bubble_->GetLabelForTesting()->GetText())  // IN-TEST
             : "";
}

}  // namespace captions
