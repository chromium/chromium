// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/accessibility/caption_bubble_context_browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace content {
class WebContents;
}

namespace captions {

class CaptionBubbleSessionObserverViews;
class CaptionBubble;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context for Views
//
//  The implementation of the Caption Bubble Context for Views.
//
class CaptionBubbleContextViews : public CaptionBubbleContextBrowser,
                                  public TabStripModelObserver {
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
  bool ShouldAvoidOverlap() const override;
  std::unique_ptr<CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() override;
  OpenCaptionSettingsCallback GetOpenCaptionSettingsCallback() override;
  void SetContextActivatabilityObserver(CaptionBubble* caption_bubble) override;
  void RemoveContextActivatabilityObserver() override;

  // TabStripModelObserver overrides:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  void OpenCaptionSettings();

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;

  raw_ptr<CaptionBubble> caption_bubble_;

  std::unique_ptr<CaptionBubbleSessionObserver> web_contents_observer_;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_VIEWS_H_
