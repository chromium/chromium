// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/web_link_preview_triggerer_impl.h"

#include "base/functional/bind.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"

namespace {

constexpr base::TimeDelta kHoverThreshold = base::Milliseconds(800);
// TODO(crbug.com/330196622):
//   Use ui::GestureConfiguration()->long_press_time_ms() here.
constexpr base::TimeDelta kLongPressThreshold = base::Milliseconds(800);

blink::WebURL GetURL(blink::WebElement& anchor_element) {
  if (anchor_element.IsNull()) {
    return blink::WebURL();
  }

  blink::WebString href = anchor_element.GetAttribute("href");
  if (href.IsNull()) {
    return blink::WebURL();
  }

  // TODO(b:325558426): Mimic HTMLAnchorElement::Href and
  // StripLeadingAndTrailingHTMLSpaces.
  blink::WebURL url = anchor_element.GetDocument().CompleteURL(href);

  if (!url.IsValid()) {
    return blink::WebURL();
  }

  return url;
}

blink::WebElement GetMostInnerAnchorElement(blink::WebElement element) {
  blink::WebElement most_inner_anchor_element;
  for (blink::WebNode curr = element; !curr.IsNull() && curr.IsElementNode();
       curr = curr.ParentNode()) {
    blink::WebElement curr_as_element = curr.To<blink::WebElement>();
    if (curr_as_element.HasHTMLTagName("a")) {
      most_inner_anchor_element = curr_as_element;
      break;
    }
  }

  return most_inner_anchor_element;
}

}  // namespace

std::unique_ptr<blink::WebLinkPreviewTriggerer>
CreateWebLinkPreviewTriggerer() {
  if (!base::FeatureList::IsEnabled(blink::features::kLinkPreview)) {
    return nullptr;
  }

  switch (blink::features::kLinkPreviewTriggerType.Get()) {
    // Alt+click is handled by navigation policy.
    case blink::features::LinkPreviewTriggerType::kAltClick:
      return nullptr;
    case blink::features::LinkPreviewTriggerType::kAltHover:
      return std::make_unique<WebLinkPreviewTriggererAltHover>();
    case blink::features::LinkPreviewTriggerType::kLongPress:
      return std::make_unique<WebLinkPreviewTriggererLongPress>();
  }
}

WebLinkPreviewTriggererAltHover::WebLinkPreviewTriggererAltHover()
    : timer_(std::make_unique<base::OneShotTimer>()) {}

WebLinkPreviewTriggererAltHover::~WebLinkPreviewTriggererAltHover() = default;

WebLinkPreviewTriggererAltHover::WebLinkPreviewTriggererAltHover(
    WebLinkPreviewTriggererAltHover&& other) = default;

WebLinkPreviewTriggererAltHover& WebLinkPreviewTriggererAltHover::operator=(
    WebLinkPreviewTriggererAltHover&& other) = default;

void WebLinkPreviewTriggererAltHover::MaybeChangedKeyEventModifier(
    int modifiers) {
  bool is_alt_on = (modifiers & blink::WebInputEvent::kAltKey) != 0;
  UpdateState(is_alt_on, anchor_element_);
}

void WebLinkPreviewTriggererAltHover::DidChangeHoverElement(
    blink::WebElement element) {
  UpdateState(is_alt_on_, GetMostInnerAnchorElement(element));
}

void WebLinkPreviewTriggererAltHover::UpdateState(
    bool is_alt_on,
    blink::WebElement anchor_element) {
  if (is_alt_on_ == is_alt_on && anchor_element_ == anchor_element) {
    return;
  }

  is_alt_on_ = is_alt_on;
  anchor_element_ = anchor_element;

  if (is_alt_on_ && !GetURL(anchor_element).IsNull()) {
    timer_->Start(
        FROM_HERE, kHoverThreshold,
        base::BindOnce(&WebLinkPreviewTriggererAltHover::InitiatePreview,
                       // base::Unretained() is safe since `this` owns `timer_`.
                       base::Unretained(this)));
  } else {
    timer_->Stop();
  }
}

void WebLinkPreviewTriggererAltHover::InitiatePreview() {
  blink::WebDocument document = anchor_element_.GetDocument();
  if (document.IsNull()) {
    return;
  }

  blink::WebURL url = GetURL(anchor_element_);
  if (url.IsNull()) {
    return;
  }

  document.InitiatePreview(url);
}

WebLinkPreviewTriggererLongPress::WebLinkPreviewTriggererLongPress()
    : timer_(std::make_unique<base::OneShotTimer>()) {}

WebLinkPreviewTriggererLongPress::~WebLinkPreviewTriggererLongPress() = default;

WebLinkPreviewTriggererLongPress::WebLinkPreviewTriggererLongPress(
    WebLinkPreviewTriggererLongPress&& other) = default;

WebLinkPreviewTriggererLongPress& WebLinkPreviewTriggererLongPress::operator=(
    WebLinkPreviewTriggererLongPress&& other) = default;

void WebLinkPreviewTriggererLongPress::DidChangeHoverElement(
    blink::WebElement element) {
  CHECK(!timer_->IsRunning() || !anchor_element_.IsNull());
  if (GetMostInnerAnchorElement(element) != anchor_element_) {
    timer_->Stop();
    anchor_element_.Reset();
  }
}

void WebLinkPreviewTriggererLongPress::DidAnchorElementReceiveMouseDownEvent(
    blink::WebElement anchor_element,
    blink::WebMouseEvent::Button button,
    int click_count) {
  CHECK(!timer_->IsRunning() || !anchor_element_.IsNull());
  timer_->Stop();
  anchor_element_.Reset();
  if (button == blink::WebMouseEvent::Button::kLeft && click_count == 1) {
    anchor_element_ = anchor_element;
    timer_->Start(
        FROM_HERE, kLongPressThreshold,
        base::BindOnce(&WebLinkPreviewTriggererLongPress::InitiatePreview,
                       // base::Unretained() is safe since `this` owns `timer_`.
                       base::Unretained(this)));
  }
}

void WebLinkPreviewTriggererLongPress::DidAnchorElementReceiveMouseUpEvent(
    blink::WebElement anchor_element,
    blink::WebMouseEvent::Button button,
    int click_count) {
  CHECK(!timer_->IsRunning() || !anchor_element_.IsNull());
  timer_->Stop();
  anchor_element_.Reset();
}

void WebLinkPreviewTriggererLongPress::InitiatePreview() {
  blink::WebDocument document = anchor_element_.GetDocument();
  blink::WebURL url = GetURL(anchor_element_);
  anchor_element_.Reset();
  if (document.IsNull() || url.IsNull()) {
    return;
  }
  document.InitiatePreview(url);
}
