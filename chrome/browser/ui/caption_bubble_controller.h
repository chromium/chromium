// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_

#include <memory>
#include <string>

#include "chrome/common/caption.mojom.h"
#include "ui/native_theme/caption_style.h"

class Browser;

namespace content {
class WebContents;
}

namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller
//
//  The interface for the caption bubble controller. It controls the caption
//  bubble. It is responsible for tasks such as post-processing of the text that
//  might need to occur before it is displayed in the bubble, and hiding or
//  showing the caption bubble when captions are received or the tab changes.
//  There exists one caption bubble controller per browser window.
//
class CaptionBubbleController {
 public:
  explicit CaptionBubbleController(Browser* browser) {}
  virtual ~CaptionBubbleController() = default;
  CaptionBubbleController(const CaptionBubbleController&) = delete;
  CaptionBubbleController& operator=(const CaptionBubbleController&) = delete;

  static std::unique_ptr<CaptionBubbleController> Create(Browser* browser);

  // Called when a transcription is received from the service. Returns whether
  // the transcription result was set on the caption bubble successfully.
  // Transcriptions will halt if this returns false.
  virtual bool OnTranscription(
      const chrome::mojom::TranscriptionResultPtr& transcription_result,
      content::WebContents* web_contents) = 0;

  // Called when the speech service has an error.
  virtual void OnError(content::WebContents* web_contents) = 0;

  // Called when the caption style changes.
  virtual void UpdateCaptionStyle(
      base::Optional<ui::CaptionStyle> caption_style) = 0;

 private:
  friend class CaptionControllerTest;

  virtual bool IsWidgetVisibleForTesting() = 0;
  virtual std::string GetBubbleLabelTextForTesting() = 0;
};

}  // namespace captions

#endif  // CHROME_BROWSER_UI_CAPTION_BUBBLE_CONTROLLER_H_
