// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
#define CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace ukm {
class MojoUkmRecorder;
}

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
  // the add-to-cart heuristics. |skip_length_limit| to decide whether to
  // crop the string to under length limit when matching.
  static bool IsAddToCart(base::StringPiece str,
                          bool skip_length_limit = false);
  // Whether the main frame URL is a shopping cart.
  static bool IsVisitCart(const GURL& main_frame_url);
  // Whether the main frame URL is a checkout page.
  static bool IsVisitCheckout(const GURL& main_frame_url);
  // Whether the main frame URL is a purchase page.
  static bool IsPurchase(const GURL& main_frame_url);
  // Whether the button text in a page with |url| corresponds to a purchase.
  static bool IsPurchase(const GURL& url, base::StringPiece button_text);
  // Whether the product should be skipped, based on product name.
  static bool ShouldSkip(base::StringPiece product_name);
  // Whether the request with navigation URL as |navigation_url| and request URL
  // as |request_url| should be skipped for AddToCart detection.
  static bool ShouldSkipAddToCartRequest(const GURL& navigation_url,
                                         const GURL& request_url);
  void OnProductsExtracted(std::unique_ptr<base::Value> result);
  static const std::vector<std::string> ExtractButtonTexts(
      const blink::WebFormElement& form);

 private:
  void MaybeExtractProducts();
  void ExtractProducts();
  void ExtractCartFromCurrentFrame();
  void ExtractCartWithUpdatedScript(
      mojo::Remote<mojom::CommerceHintObserver> observer,
      const std::string& product_id_json,
      const std::string& cart_extraction_script);
  class JavaScriptRequest;

  JavaScriptRequest* javascript_request_{nullptr};
  GURL starting_url_;
  bool has_finished_loading_{false};
  int extraction_count_{0};
  bool is_extraction_pending_{false};
  bool is_extraction_running_{false};
  bool should_skip_{false};
  bool extraction_script_initialized_{false};
  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;
  base::WeakPtrFactory<CommerceHintAgent> weak_factory_{this};

  class JavaScriptRequest : public blink::WebScriptExecutionCallback {
   public:
    explicit JavaScriptRequest(base::WeakPtr<CommerceHintAgent> agent);
    JavaScriptRequest(const JavaScriptRequest&) = delete;
    JavaScriptRequest& operator=(const JavaScriptRequest&) = delete;
    void WillExecute() override;
    void Completed(
        const blink::WebVector<v8::Local<v8::Value>>& result) override;

   private:
    ~JavaScriptRequest() override;
    base::WeakPtr<CommerceHintAgent> agent_;
    base::TimeTicks start_time_;
  };

  // content::RenderFrameObserver overrides
  void OnDestruct() override;
  void WillSendRequest(const blink::WebURLRequest& request) override;
  void DidStartNavigation(
      const GURL& url,
      absl::optional<blink::WebNavigationType> navigation_type) override;
  void DidCommitProvisionalLoad(ui::PageTransition transition) override;
  void DidFinishLoad() override;
  void WillSubmitForm(const blink::WebFormElement& form) override;
  void DidObserveLayoutShift(double score, bool after_input_or_scroll) override;
  void OnMainFrameIntersectionChanged(const gfx::Rect& intersect_rect) override;

  // Callbacks with business logics for handling navigation-related observer
  // calls. These callbacks are triggered when navigation-related signals are
  // captured and carry (1) a bool `should_skip` indicating whether commerce
  // hint signals should be collected on current URL or not. (2) `heuristics`
  // carrying commerce heuristics that are applicable in current domain.
  void DidStartNavigationCallback(
      const GURL& url,
      mojo::Remote<mojom::CommerceHintObserver> observer,
      bool should_skip,
      mojom::HeuristicsPtr heuristics);
  void DidFinishLoadCallback(const GURL& url,
                             mojo::Remote<mojom::CommerceHintObserver> observer,
                             bool should_skip,
                             mojom::HeuristicsPtr heuristics);
};

}  // namespace cart

#endif  // CHROME_RENDERER_CART_COMMERCE_HINT_AGENT_H_
