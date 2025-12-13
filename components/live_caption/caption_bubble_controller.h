// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_
#define COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "components/live_caption/caption_controller_base.h"
#include "components/live_caption/views/caption_bubble.h"
#include "ui/native_theme/caption_style.h"

namespace content {
class BrowserContext;
}

namespace captions {

class CaptionBubbleContext;
class CaptionBubbleSettings;
class TranslationViewWrapperBase;

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Controller
//
//  The interface for the caption bubble controller. It controls the caption
//  bubble. It is responsible for tasks such as post-processing of the text that
//  might need to occur before it is displayed in the bubble, and hiding or
//  showing the caption bubble when captions are received. There exists one
//  caption bubble controller per profile.
//
class CaptionBubbleController : public CaptionControllerBase::Listener {
 public:
  explicit CaptionBubbleController() = default;
  ~CaptionBubbleController() override = default;
  CaptionBubbleController(const CaptionBubbleController&) = delete;
  CaptionBubbleController& operator=(const CaptionBubbleController&) = delete;

  static std::unique_ptr<CaptionBubbleController> Create(
      CaptionBubbleSettings* caption_bubble_settings,
      const std::string& application_locale,
      std::unique_ptr<TranslationViewWrapperBase> translation_view_wrapper);

  // Called when the speech service has an error.  This should be part of
  // `CaptionControllerBase::Listener`, but the callbacks make this tricky.
  virtual void OnError(
      CaptionBubbleContext* caption_bubble_context,
      CaptionBubbleErrorType error_type,
      OnErrorClickedCallback error_clicked_callback,
      OnDoNotShowAgainClickedCallback error_silenced_callback) = 0;

  // Called when the caption style changes.
  virtual void UpdateCaptionStyle(
      std::optional<ui::CaptionStyle> caption_style) = 0;

  virtual bool IsWidgetVisibleForTesting() = 0;
  virtual bool IsGenericErrorMessageVisibleForTesting() = 0;
  virtual std::string GetBubbleLabelTextForTesting() = 0;
  virtual void CloseActiveModelForTesting() = 0;
};

}  // namespace captions

#endif  // COMPONENTS_LIVE_CAPTION_CAPTION_BUBBLE_CONTROLLER_H_
