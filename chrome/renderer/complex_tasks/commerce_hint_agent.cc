// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/complex_tasks/commerce_hint_agent.h"

#include "base/json/json_writer.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/common/complex_tasks/commerce_hints.mojom.h"
#include "chrome/grit/renderer_resources.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"

using base::UserMetricsAction;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebString;

namespace complex_tasks {

namespace {

constexpr unsigned kLengthLimit = 1024;
constexpr char kAmazonDomain[] = "amazon.com";

// TODO: dedup with chrome/browser/complex_tasks/commerce_hint_service.cc
std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

enum class CommerceEvent {
  kAddToCartByForm,
  kAddToCartByURL,
  kVisitCart,
  kVisitCheckout,
  kPurchaseByForm,
  kPurchaseByURL,
};

void RecordAction(const UserMetricsAction& action) {
  content::RenderThread::Get()->RecordAction(action);
}

void RecordCommerceEvent(CommerceEvent event) {
  switch (event) {
    case CommerceEvent::kAddToCartByForm:
      VLOG(1) << "Commerce.AddToCart by POST form";
      RecordAction(UserMetricsAction("Commerce.AddToCart"));
      break;
    case CommerceEvent::kAddToCartByURL:
      VLOG(1) << "Commerce.AddToCart by URL";
      RecordAction(UserMetricsAction("Commerce.AddToCart"));
      break;
    case CommerceEvent::kVisitCart:
      VLOG(1) << "Commerce.VisitCart";
      RecordAction(UserMetricsAction("Commerce.VisitCart"));
      break;
    case CommerceEvent::kVisitCheckout:
      VLOG(1) << "Commerce.VisitCheckout";
      RecordAction(UserMetricsAction("Commerce.VisitCheckout"));
      break;
    case CommerceEvent::kPurchaseByForm:
      VLOG(1) << "Commerce.Purchase by POST form";
      RecordAction(UserMetricsAction("Commerce.Purchase"));
      break;
    case CommerceEvent::kPurchaseByURL:
      VLOG(1) << "Commerce.Purchase by URL";
      RecordAction(UserMetricsAction("Commerce.Purchase"));
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

void OnVisitCart(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  if (!observer.is_bound())
    return;
  observer->OnVisitCart();
}

void OnCartProductUpdated(content::RenderFrame* render_frame,
                          std::vector<mojom::ProductPtr> products) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  if (!observer.is_bound())
    return;
  observer->OnCartProductUpdated(std::move(products));
}

void OnVisitCheckout(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  if (!observer.is_bound())
    return;
  observer->OnVisitCart();
}

void OnPurchase(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  if (!observer.is_bound())
    return;
  observer->OnVisitCart();
}

bool PartialMatch(base::StringPiece str, const re2::RE2& re) {
  return RE2::PartialMatch(re2::StringPiece(str.data(), str.size()), re);
}

// The detection of "AddToCart", "VisitCart", "VisitCheckout", and "Purchase"
// currently only support 1 marketplace and 2 E-commerce platforms.
// TODO(crbug/1103450): Enhance coverage of more marketplaces and E-commerce
// platforms.

const re2::RE2& GetAddToCartPattern() {
  static base::NoDestructor<re2::RE2> instance(
      "(\\b|[^a-zA-Z])"
      "((add[_-]?to[_-]?(cart|basket))|(cart\\/add))"
      "(\\b|[^a-zA-Z])");
  return *instance;
}

const re2::RE2& GetVisitCartPatternAmazon() {
  static base::NoDestructor<re2::RE2> instance(
      "^/(-/[A-Za-z_-]+/)?gp/((.*/)?cart(/.*)?)(/|$)");
  return *instance;
}

const re2::RE2& GetVisitCartPattern() {
  static base::NoDestructor<re2::RE2> instance("/(my|co-)?cart(/|$)");
  return *instance;
}

const re2::RE2& GetVisitCheckoutPattern() {
  static base::NoDestructor<re2::RE2> instance("/checkouts?(/|$)");
  return *instance;
}

const re2::RE2& GetPurchaseTextPattern() {
  static base::NoDestructor<re2::RE2> instance(
      "^(?i)((pay now)|(place order))$");
  return *instance;
}

bool IsSameDomainXHR(const std::string& host,
                     const blink::WebURLRequest& request) {
  // Only handle XHR POST requests here.
  // Other matches like navigation is handled in DidStartNavigation().
  if (!request.HttpMethod().Equals("POST"))
    return false;

  GURL url = request.Url();
  return url.DomainIs(host);
}

void DetectAddToCart(content::RenderFrame* render_frame,
                     const blink::WebURLRequest& request) {
  // Only handle XHR POST requests here.
  // Other matches like navigation is handled in DidStartNavigation().
  if (!request.HttpMethod().Equals("POST"))
    return;

  GURL url = request.Url();
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
    if (element.type != blink::WebHTTPBody::Element::kTypeData)
      continue;

    // TODO(crbug/1103450): this copy is avoidable if element is guaranteed to
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

// Works on top 10 desktop merchants.
bool CommerceHintAgent::IsVisitCart(const GURL& url) {
  if (eTLDPlusOne(url) == kAmazonDomain) {
    return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                        GetVisitCartPatternAmazon()) ||
           url.path_piece() == "/gp/aw/c";
  }
  return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                      GetVisitCartPattern()) ||
         base::StartsWith(url.host_piece(), "cart");
}

bool CommerceHintAgent::IsVisitCheckout(const GURL& url) {
  if (url.DomainIs(kAmazonDomain)) {
    return base::StartsWith(url.path_piece(),
                            "/gp/cart/mobile/go-to-checkout.html");
  }
  return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                      GetVisitCheckoutPattern());
}

bool CommerceHintAgent::IsPurchase(const GURL& url) {
  if (url.DomainIs(kAmazonDomain)) {
    return base::StartsWith(
        url.path_piece(), "/gp/buy/spc/handlers/static-submit-decoupled.html");
  }
  return false;
}

bool CommerceHintAgent::IsPurchase(base::StringPiece str) {
  return PartialMatch(str, GetPurchaseTextPattern());
}

std::string CommerceHintAgent::ExtractButtonText(
    const blink::WebFormElement& form) {
  static base::NoDestructor<WebString> kButton("button");

  const WebElementCollection& buttons = form.GetElementsByHTMLTagName(*kButton);
  if (buttons.length() != 1)
    return base::EmptyString();
  const WebElement& button = buttons.FirstItem();
  // TODO(crbug/1103450): emulate innerText to be more robust.
  return std::string(base::TrimWhitespaceASCII(button.TextContent().Utf8(),
                                               base::TrimPositions::TRIM_ALL));
}

void CommerceHintAgent::ExtractProducts() {
  blink::WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_MERCHANT_LIST_EXTRACTION_JS);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  blink::WebScriptSource source =
      blink::WebScriptSource(WebString::FromASCII(script));

  // TODO(wychen): use
  // RequestExecuteScriptInIsolatedWorld(ISOLATED_WORLD_ID_CHROME_INTERNAL,
  // WebLocalFrame::kAsynchronous) instead.
  v8::Local<v8::Value> result = main_frame->ExecuteScriptAndReturnValue(source);
  if (result.IsEmpty())
    return;

  OnProductsExtracted(content::V8ValueConverter::Create()->FromV8Value(
      result, main_frame->MainWorldScriptContext()));
}

void CommerceHintAgent::OnProductsExtracted(
    std::unique_ptr<base::Value> results) {
  if (!results) {
    LOG(ERROR) << "OnProductsExtracted() got empty results";
    return;
  }
  std::string json;
  base::JSONWriter::Write(*results, &json);
  VLOG(2) << "OnProductsExtracted: " << json;

  if (!results->is_list())
    return;
  std::vector<mojom::ProductPtr> products;
  for (const auto& product : results->GetList()) {
    if (!product.is_dict())
      continue;
    const auto* image_url = product.FindKey("imageUrl");
    VLOG(1) << "image_url = " << image_url->GetString();
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->image_url = image_url->GetString();
    products.push_back(std::move(product_ptr));
  }
  OnCartProductUpdated(render_frame(), std::move(products));
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  DetectAddToCart(render_frame(), request);

  // Detect XHR in cart page.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  GURL url(frame->GetDocument().Url());

  if (IsVisitCart(url) && IsSameDomainXHR(url.host(), request)) {
    // TODO: this extraction might be too excessive.
    VLOG(1) << "In-cart XHR: " << request.Url();
    ExtractProducts();
  }
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  if (IsAddToCart(url.PathForRequestPiece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame());
  }
  if (IsVisitCheckout(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCheckout);
    OnVisitCheckout(render_frame());
  }
  if (IsPurchase(url)) {
    RecordCommerceEvent(CommerceEvent::kPurchaseByURL);
    OnPurchase(render_frame());
  }
}

void CommerceHintAgent::DidFinishLoad() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  GURL url(frame->GetDocument().Url());

  if (IsVisitCart(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCart);
    OnVisitCart(render_frame());
    ExtractProducts();
  }
}

void CommerceHintAgent::WillSubmitForm(const blink::WebFormElement& form) {
  if (IsPurchase(ExtractButtonText(form))) {
    RecordCommerceEvent(CommerceEvent::kPurchaseByForm);
    OnPurchase(render_frame());
  }
}

void CommerceHintAgent::DidObserveLayoutShift(double score,
                                              bool after_input_or_scroll) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  GURL url(frame->GetDocument().Url());

  if (IsVisitCart(url)) {
    // TODO: this extraction might be too excessive.
    ExtractProducts();
  }
}
}  // namespace complex_tasks
