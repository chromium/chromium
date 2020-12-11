// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_MODEL_H_

#include <string>

#include "content/public/browser/web_contents_observer.h"

namespace captions {

class CaptionBubble;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Model
//
//  A representation of the data a caption bubble needs on a particular web
//  contents. The caption bubble controller sets the value of the text. The
//  caption bubble observes the model, and when the values change, the observer
//  is alerted.
//
//  There exists one CaptionBubble and one CaptionBubbleControllerViews per
//  browser, but one CaptionBubbleModel per tab. The CaptionBubbleModel is owned
//  by the CaptionBubbleControllerViews. It is created when a tab activates and
//  exists for the lifetime of that tab.
//
//  Partial text is a speech result that is subject to change. Incoming partial
//  texts overlap with the previous partial text.
//  Final text is the final transcription from the speech service that no
//  longer changes. Incoming partial texts do not overlap with final text.
//  When a final result is received from the speech service, the partial text is
//  appended to the end of the final text. The caption bubble displays the full
//  final + partial text.
//
class CaptionBubbleModel : public content::WebContentsObserver {
 public:
  explicit CaptionBubbleModel(content::WebContents* web_contents);
  ~CaptionBubbleModel() override;
  CaptionBubbleModel(const CaptionBubbleModel&) = delete;
  CaptionBubbleModel& operator=(const CaptionBubbleModel&) = delete;

  void SetObserver(CaptionBubble* observer);
  void RemoveObserver();

  // Set the partial text and alert the observer.
  void SetPartialText(const std::string& partial_text);

  // Commits the partial text as final text.
  void CommitPartialText();

  // Set that the bubble has an error and alert the observer.
  void OnError();

  // Mark the bubble as closed, clear the partial and final text, and alert the
  // observer.
  void Close();

  bool IsClosed() const { return is_closed_; }
  bool HasError() const { return has_error_; }
  std::string GetFullText() const { return final_text_ + partial_text_; }

 protected:
  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  // Alert the observer that a change has occurred to the model text.
  void OnTextChanged();

  std::string final_text_;
  std::string partial_text_;

  // Whether the bubble has been closed by the user.
  bool is_closed_ = false;

  // Whether an error should be displayed one the bubble.
  bool has_error_ = false;

  // The CaptionBubble observing changes to this model.
  CaptionBubble* observer_ = nullptr;
};
}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_MODEL_H_
