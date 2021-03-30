// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/caption_bubble_controller_views.h"

#include <memory>
#include <string>

#include "chrome/browser/accessibility/caption_controller.h"
#include "chrome/browser/accessibility/caption_controller_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble.h"
#include "chrome/browser/ui/views/accessibility/caption_bubble_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"

namespace captions {

// Static
std::unique_ptr<CaptionBubbleController> CaptionBubbleController::Create(
    Browser* browser) {
  return std::make_unique<CaptionBubbleControllerViews>(browser);
}

// Static
views::View* CaptionBubbleControllerViews::GetCaptionBubbleAccessiblePane(
    Browser* browser) {
  CaptionController* caption_controller =
      CaptionControllerFactory::GetForProfileIfExists(browser->profile());
  if (caption_controller) {
    CaptionBubbleControllerViews* bubble_controller =
        static_cast<CaptionBubbleControllerViews*>(
            caption_controller->GetCaptionBubbleControllerForBrowser(browser));
    if (bubble_controller)
      return bubble_controller->GetFocusableCaptionBubble();
  }
  return nullptr;
}

CaptionBubbleControllerViews::CaptionBubbleControllerViews(Browser* browser)
    : CaptionBubbleController(browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  caption_bubble_ = new CaptionBubble(
      browser_view->GetContentsView(), browser_view,
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
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
    CaptionHostImpl* caption_host_impl,
    const chrome::mojom::TranscriptionResultPtr& transcription_result) {
  if (!caption_bubble_)
    return false;
  SetActiveModel(caption_host_impl);
  if (active_model_->IsClosed())
    return false;

  // If the caption bubble has no activity and it receives a final
  // transcription, don't set text. The speech service sends a final
  // transcription after several seconds of no audio. This prevents the bubble
  // reappearing with a final transcription after it had disappeared due to no
  // activity.
  if (!caption_bubble_->HasActivity() && transcription_result->is_final)
    return true;

  active_model_->SetPartialText(transcription_result->transcription);
  if (transcription_result->is_final)
    active_model_->CommitPartialText();

  return true;
}

void CaptionBubbleControllerViews::OnError(CaptionHostImpl* caption_host_impl) {
  if (!caption_bubble_)
    return;
  SetActiveModel(caption_host_impl);
  if (active_model_->IsClosed())
    return;
  active_model_->OnError();
}

void CaptionBubbleControllerViews::OnAudioStreamEnd(
    CaptionHostImpl* caption_host_impl) {
  if (!caption_bubble_)
    return;

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[caption_host_impl].get();
  if (active_model_ == caption_bubble_model) {
    active_model_ = nullptr;
    caption_bubble_->SetModel(nullptr);
  }
  caption_bubble_models_.erase(caption_host_impl);
}

void CaptionBubbleControllerViews::UpdateCaptionStyle(
    base::Optional<ui::CaptionStyle> caption_style) {
  caption_bubble_->UpdateCaptionStyle(caption_style);
}

views::View* CaptionBubbleControllerViews::GetFocusableCaptionBubble() {
  if (caption_widget_ && caption_widget_->IsVisible())
    return caption_bubble_;
  return nullptr;
}

void CaptionBubbleControllerViews::SetActiveModel(
    CaptionHostImpl* caption_host_impl) {
  if (!caption_bubble_models_.count(caption_host_impl)) {
    caption_bubble_models_.emplace(caption_host_impl,
                                   std::make_unique<CaptionBubbleModel>());
  }

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[caption_host_impl].get();
  if (active_model_ != caption_bubble_model) {
    active_model_ = caption_bubble_model;
    caption_bubble_->SetModel(active_model_);
  }
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
