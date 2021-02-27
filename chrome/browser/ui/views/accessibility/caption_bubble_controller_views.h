// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "chrome/browser/ui/caption_bubble_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace views {
class View;
class Widget;
}

namespace captions {

class CaptionBubble;
class CaptionBubbleModel;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller for Views
//
//  The implementation of the caption bubble controller for Views.
//
class CaptionBubbleControllerViews : public CaptionBubbleController,
                                     public TabStripModelObserver {
 public:
  static views::View* GetCaptionBubbleAccessiblePane(Browser* browser);

  explicit CaptionBubbleControllerViews(Browser* browser);
  ~CaptionBubbleControllerViews() override;
  CaptionBubbleControllerViews(const CaptionBubbleControllerViews&) = delete;
  CaptionBubbleControllerViews& operator=(const CaptionBubbleControllerViews&) =
      delete;

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  bool OnTranscription(
      const chrome::mojom::TranscriptionResultPtr& transcription_result,
      content::WebContents* web_contents) override;

  // Called when the speech service has an error.
  void OnError(content::WebContents* web_contents) override;

  // Called when the caption style changes.
  void UpdateCaptionStyle(
      base::Optional<ui::CaptionStyle> caption_style) override;

  // Returns the view of the caption bubble which should receive focus, if one
  // exists.
  views::View* GetFocusableCaptionBubble();

 private:
  friend class CaptionBubbleControllerViewsTest;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // A callback passed to the CaptionBubble which is called when the
  // CaptionBubble is destroyed.
  void OnCaptionBubbleDestroyed();

  // Sets the active contents to the given web contents, and creates a new
  // CaptionBubbleModel for that contents if one does not already exist.
  void SetActiveContents(content::WebContents* contents);

  bool IsWidgetVisibleForTesting() override;
  std::string GetBubbleLabelTextForTesting() override;

  CaptionBubble* caption_bubble_;
  views::Widget* caption_widget_;
  Browser* browser_;

  // The web contents corresponding to the active tab.
  content::WebContents* active_contents_;

  // A map of web contents and their corresponding CaptionBubbleModel. New
  // entries are added to this map when a previously un-activated web contents
  // is activated.
  std::unordered_map<content::WebContents*, std::unique_ptr<CaptionBubbleModel>>
      caption_bubble_models_;
};
}  // namespace captions

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBILITY_CAPTION_BUBBLE_CONTROLLER_VIEWS_H_
