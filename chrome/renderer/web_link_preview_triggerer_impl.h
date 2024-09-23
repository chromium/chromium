// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEB_LINK_PREVIEW_TRIGGERER_IMPL_H_
#define CHROME_RENDERER_WEB_LINK_PREVIEW_TRIGGERER_IMPL_H_

#include "base/timer/timer.h"
#include "third_party/blink/public/web/web_link_preview_triggerer.h"

// Creates appropriate WebLinkPreviewTriggerer depending on feature flag and
// params.
std::unique_ptr<blink::WebLinkPreviewTriggerer> CreateWebLinkPreviewTriggerer();

// Observes events in frame and triggers Link Preview: Alt+hover trigger.
//
// This class tracks the state "is Alt key pressed" and mouse hovered anchor
// element. Alt key tracking is done by observing modifiers for each keyboard
// events and mouse leave event. See the comment of `last_key_event_modifiers_`
// for more details and limitations.
class WebLinkPreviewTriggererAltHover final
    : public blink::WebLinkPreviewTriggerer {
 public:
  WebLinkPreviewTriggererAltHover();

  ~WebLinkPreviewTriggererAltHover() override;

  // Movable but not copyable.
  WebLinkPreviewTriggererAltHover(WebLinkPreviewTriggererAltHover&& other);
  WebLinkPreviewTriggererAltHover& operator=(
      WebLinkPreviewTriggererAltHover&& other);
  WebLinkPreviewTriggererAltHover(const WebLinkPreviewTriggererAltHover&) =
      delete;
  WebLinkPreviewTriggererAltHover& operator=(
      const WebLinkPreviewTriggererAltHover&) = delete;

  // Implements blink::WebLinkPreviewTriggerer.
  void MaybeChangedKeyEventModifier(int modifiers) override;
  void DidChangeHoverElement(blink::WebElement element) override;

 private:
  void UpdateState(bool is_alt_on, blink::WebElement anchor_element);

  void InitiatePreview();

  std::unique_ptr<base::OneShotTimer> timer_;

  bool is_alt_on_ = false;
  blink::WebElement anchor_element_;
};

// Observes events in frame and triggers Link Preview: Long press trigger.
class WebLinkPreviewTriggererLongPress final
    : public blink::WebLinkPreviewTriggerer {
 public:
  WebLinkPreviewTriggererLongPress();

  ~WebLinkPreviewTriggererLongPress() override;

  // Movable but not copyable.
  WebLinkPreviewTriggererLongPress(WebLinkPreviewTriggererLongPress&& other);
  WebLinkPreviewTriggererLongPress& operator=(
      WebLinkPreviewTriggererLongPress&& other);
  WebLinkPreviewTriggererLongPress(const WebLinkPreviewTriggererLongPress&) =
      delete;
  WebLinkPreviewTriggererLongPress& operator=(
      const WebLinkPreviewTriggererLongPress&) = delete;

  // Implements blink::WebLinkPreviewTriggerer.
  void DidChangeHoverElement(blink::WebElement element) override;
  void DidAnchorElementReceiveMouseDownEvent(
      blink::WebElement anchor_element,
      blink::WebMouseEvent::Button button,
      int click_count) override;
  void DidAnchorElementReceiveMouseUpEvent(blink::WebElement anchor_element,
                                           blink::WebMouseEvent::Button button,
                                           int click_count) override;

 private:
  void InitiatePreview();

  std::unique_ptr<base::OneShotTimer> timer_;

  blink::WebElement anchor_element_;
};

#endif  // CHROME_RENDERER_WEB_LINK_PREVIEW_TRIGGERER_IMPL_H_
