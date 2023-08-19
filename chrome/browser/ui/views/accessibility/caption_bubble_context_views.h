// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_

#include "chrome/browser/accessibility/caption_bubble_context_browser.h"

#include <string>

#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}

namespace captions {

class CaptionBubbleSessionObserverViews;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context for Views
//
//  The implementation of the Caption Bubble Context for Views.
//
class CaptionBubbleContextViews : public CaptionBubbleContextBrowser {
 public:
  explicit CaptionBubbleContextViews(content::WebContents* web_contents);
  ~CaptionBubbleContextViews() override;
  CaptionBubbleContextViews(const CaptionBubbleContextViews&) = delete;
  CaptionBubbleContextViews& operator=(const CaptionBubbleContextViews&) =
      delete;

  // CaptionBubbleContextBrowser:
  void GetBounds(GetBoundsCallback callback) const override;
  const std::string GetSessionId() const override;
  void Activate() override;
  bool IsActivatable() const override;
  std::unique_ptr<CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() override;
  OpenCaptionSettingsCallback GetOpenCaptionSettingsCallback() override;

 private:
  void OpenCaptionSettings();

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;

  std::unique_ptr<CaptionBubbleSessionObserver> web_contents_observer_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
