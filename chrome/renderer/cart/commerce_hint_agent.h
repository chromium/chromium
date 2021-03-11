// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
#define CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_

#include "base/memory/weak_ptr.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"

namespace cart {

// The renderer-side agent for CommerceHint.
class CommerceHintAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CommerceHintAgent> {
 public:
  explicit CommerceHintAgent(content::RenderFrame* render_frame);
  ~CommerceHintAgent() override;

  CommerceHintAgent(const CommerceHintAgent&) = delete;
  CommerceHintAgent& operator=(const CommerceHintAgent&) = delete;

  // Whether the string, either from path of URL or XHR form contents, matches
  // the add-to-cart heuristics.
  static bool IsAddToCart(base::StringPiece str);

 private:
  base::WeakPtrFactory<CommerceHintAgent> weak_factory_{this};

  // content::RenderFrameObserver overrides
  void OnDestruct() override;
  void WillSendRequest(const blink::WebURLRequest& request) override;
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
};

}  // namespace cart

#endif  // CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
