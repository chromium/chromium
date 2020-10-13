// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_COMPLEX_TASKS_COMMERCE_HINT_AGENT_H_
#define CHROME_RENDERER_COMPLEX_TASKS_COMMERCE_HINT_AGENT_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "url/gurl.h"

namespace complex_tasks {

// The renderer-side agent for CommerceHint.
class CommerceHintAgent
    : public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<CommerceHintAgent> {
 public:
  explicit CommerceHintAgent(content::RenderFrame* render_frame);
  ~CommerceHintAgent() override;

  CommerceHintAgent(const CommerceHintAgent&) = delete;
  CommerceHintAgent& operator=(const CommerceHintAgent&) = delete;

  static bool IsAddToCart(base::StringPiece str);
  static bool IsVisitCart(const GURL& url);
  static bool IsVisitCheckout(const GURL& url);
  static bool IsPurchase(const GURL& url);
  static bool IsPurchase(base::StringPiece str);
  void ExtractProducts();
  void OnProductsExtracted(std::unique_ptr<base::Value> result);
  static std::string ExtractButtonText(const blink::WebFormElement& form);

 private:
  base::WeakPtrFactory<CommerceHintAgent> weak_factory_{this};

  // content::RenderFrameObserver overrides
  void OnDestruct() override;
  void WillSendRequest(const blink::WebURLRequest& request) override;
  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override;
  void DidFinishLoad() override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
};

}  // namespace complex_tasks

#endif  // CHROME_RENDERER_COMPLEX_TASKS_COMMERCE_HINT_AGENT_H_
