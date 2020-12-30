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
#include "content/public/browser/web_contents.h"

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
    : CaptionBubbleController(browser), browser_(browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  caption_bubble_ = new CaptionBubble(
      browser_view->GetContentsView(), browser_view,
      base::BindOnce(&CaptionBubbleControllerViews::OnCaptionBubbleDestroyed,
                     base::Unretained(this)));
  caption_widget_ =
      views::BubbleDialogDelegateView::CreateBubble(caption_bubble_);
  browser_->tab_strip_model()->AddObserver(this);
  SetActiveContents(browser_->tab_strip_model()->GetActiveWebContents());
}

CaptionBubbleControllerViews::~CaptionBubbleControllerViews() {
  if (caption_widget_)
    caption_widget_->CloseNow();
  if (browser_) {
    DCHECK(browser_->tab_strip_model());
    browser_->tab_strip_model()->RemoveObserver(this);
  }
}

void CaptionBubbleControllerViews::OnCaptionBubbleDestroyed() {
  caption_bubble_ = nullptr;
  caption_widget_ = nullptr;

  // The caption bubble is destroyed when the browser is destroyed. So if the
  // caption bubble was destroyed, then browser_ must also be nullptr.
  browser_ = nullptr;
}

bool CaptionBubbleControllerViews::OnTranscription(
    const chrome::mojom::TranscriptionResultPtr& transcription_result,
    content::WebContents* web_contents) {
  if (!caption_bubble_ || !caption_bubble_models_.count(web_contents) ||
      caption_bubble_models_[web_contents]->IsClosed())
    return false;

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[web_contents].get();

  // If the caption bubble has no activity and it receives a final
  // transcription, don't set text. The speech service sends a final
  // transcription after several seconds of no audio. This prevents the bubble
  // reappearing with a final transcription after it had disappeared due to no
  // activity.
  if (!caption_bubble_->HasActivity() && transcription_result->is_final)
    return true;

  caption_bubble_model->SetPartialText(transcription_result->transcription);
  if (transcription_result->is_final)
    caption_bubble_model->CommitPartialText();

  return true;
}

void CaptionBubbleControllerViews::OnError(content::WebContents* web_contents) {
  if (!caption_bubble_ || !caption_bubble_models_.count(web_contents) ||
      caption_bubble_models_[web_contents]->IsClosed())
    return;

  CaptionBubbleModel* caption_bubble_model =
      caption_bubble_models_[web_contents].get();
  caption_bubble_model->OnError();
}

void CaptionBubbleControllerViews::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!caption_bubble_ || !caption_widget_)
    return;
  if (!selection.active_tab_changed())
    return;
  if (selection.selected_tabs_were_removed)
    caption_bubble_models_.erase(selection.old_contents);
  SetActiveContents(selection.new_contents);
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

void CaptionBubbleControllerViews::SetActiveContents(
    content::WebContents* contents) {
  active_contents_ = contents;
  if (!active_contents_) {
    caption_bubble_->SetModel(nullptr);
    return;
  }
  if (!caption_bubble_models_.count(active_contents_)) {
    caption_bubble_models_.emplace(
        active_contents_,
        std::make_unique<CaptionBubbleModel>(active_contents_));
  }
  caption_bubble_->SetModel(caption_bubble_models_[active_contents_].get());
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
