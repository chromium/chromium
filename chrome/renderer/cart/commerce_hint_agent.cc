// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "base/no_destructor.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/re2/src/re2/re2.h"

using base::UserMetricsAction;

namespace cart {

namespace {

constexpr unsigned kLengthLimit = 4096;
constexpr char kEbayDomain[] = "ebay.com";

enum class CommerceEvent {
  kAddToCartByForm,
  kAddToCartByURL,
};

void RecordCommerceEvent(CommerceEvent event) {
  switch (event) {
    case CommerceEvent::kAddToCartByForm:
      VLOG(1) << "Commerce.AddToCart by POST form";
      break;
    case CommerceEvent::kAddToCartByURL:
      VLOG(1) << "Commerce.AddToCart by URL";
      break;
    default:
      NOTREACHED();
  }
}

mojo::Remote<mojom::CommerceHintObserver> GetObserver(
    content::RenderFrame* render_frame) {
  // Connect to Mojo service on browser to notify commerce signals.
  mojo::Remote<mojom::CommerceHintObserver> observer;
  render_frame->GetBrowserInterfaceBroker()->GetInterface(
      observer.BindNewPipeAndPassReceiver());
  return observer;
}

void OnAddToCart(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  if (!observer.is_bound())
    return;
  observer->OnAddToCart();
}

bool PartialMatch(base::StringPiece str, const re2::RE2& re) {
  return RE2::PartialMatch(re2::StringPiece(str.data(), str.size()), re);
}

// This is based on top 30 US shopping sites.
// TODO(crbug/1164236): cover more shopping sites.
const re2::RE2& GetAddToCartPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      "(\\b|[^a-z])"
      "((add(ed)?(-|_|(%20))?(item)?(-|_|(%20))?to(-|_|(%20))?(cart|basket|bag)"
      ")|(cart\\/add)|(checkout\\/basket)|(cart_type))"
      "(\\b|[^a-z])",
      options);
  return *instance;
}

void DetectAddToCart(content::RenderFrame* render_frame,
                     const blink::WebURLRequest& request) {
  GURL url = request.Url();
  // Only handle XHR POST requests here.
  // Other matches like navigation is handled in DidStartNavigation().
  // Some sites use GET requests though, so special-case them here.
  if (!request.HttpMethod().Equals("POST") && !url.DomainIs(kEbayDomain)) {
    return;
  }

  if (CommerceHintAgent::IsAddToCart(url.path_piece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame);
    return;
  }

  blink::WebHTTPBody body = request.HttpBody();
  if (body.IsNull())
    return;

  unsigned i = 0;
  blink::WebHTTPBody::Element element;
  while (body.ElementAt(i++, element)) {
    if (element.type != blink::HTTPBodyElementType::kTypeData)
      continue;

    // TODO(crbug/1168704): this copy is avoidable if element is guaranteed to
    // have contiguous buffer.
    std::vector<uint8_t> buf = element.data.Copy().ReleaseVector();
    base::StringPiece str(reinterpret_cast<char*>(buf.data()), buf.size());

    if (CommerceHintAgent::IsAddToCart(str)) {
      RecordCommerceEvent(CommerceEvent::kAddToCartByForm);
      OnAddToCart(render_frame);
      return;
    }
  }
}

}  // namespace

CommerceHintAgent::CommerceHintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CommerceHintAgent>(render_frame) {
  DCHECK(render_frame);
}

CommerceHintAgent::~CommerceHintAgent() = default;

bool CommerceHintAgent::IsAddToCart(base::StringPiece str) {
  return PartialMatch(str.substr(0, kLengthLimit), GetAddToCartPattern());
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  DetectAddToCart(render_frame(), request);
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  if (IsAddToCart(url.PathForRequestPiece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame());
  }
}

}  // namespace cart
