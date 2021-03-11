// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "base/json/json_writer.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/renderer_resources.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/loader/http_body_element_type.h"
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

namespace cart {

namespace {

constexpr unsigned kLengthLimit = 4096;
constexpr char kAmazonDomain[] = "amazon.com";
constexpr char kEbayDomain[] = "ebay.com";
constexpr char kAppleDomain[] = "apple.com";
constexpr char kMacysDomain[] = "macys.com";

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

void RecordCommerceEvent(CommerceEvent event) {
  switch (event) {
    case CommerceEvent::kAddToCartByForm:
      DVLOG(1) << "Commerce.AddToCart by POST form";
      break;
    case CommerceEvent::kAddToCartByURL:
      DVLOG(1) << "Commerce.AddToCart by URL";
      break;
    case CommerceEvent::kVisitCart:
      DVLOG(1) << "Commerce.VisitCart";
      break;
    case CommerceEvent::kVisitCheckout:
      DVLOG(1) << "Commerce.VisitCheckout";
      break;
    case CommerceEvent::kPurchaseByForm:
      DVLOG(1) << "Commerce.Purchase by POST form";
      break;
    case CommerceEvent::kPurchaseByURL:
      DVLOG(1) << "Commerce.Purchase by URL";
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

base::Optional<GURL> ScanCartURL(content::RenderFrame* render_frame) {
  blink::WebDocument doc = render_frame->GetWebFrame()->GetDocument();

  base::Optional<GURL> best;
  blink::WebVector<WebElement> elements =
      doc.QuerySelectorAll(WebString("a[href]"));
  for (WebElement element : elements) {
    GURL link = doc.CompleteURL(element.GetAttribute("href"));
    if (!link.is_valid())
      continue;
    link = link.GetAsReferrer();
    // Only keep the shortest match. First match or most frequent match might
    // work better, but we need larger validating corpus.
    if (best && link.spec().size() >= best->spec().size())
      continue;
    if (!CommerceHintAgent::IsVisitCart(link))
      continue;
    DVLOG(2) << "Cart link: " << link;
    best = link;
  }
  if (best)
    DVLOG(1) << "Best cart link: " << *best;
  return best;
}

void OnAddToCart(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnAddToCart(ScanCartURL(render_frame));
}

void OnVisitCart(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnVisitCart();
}

void OnCartProductUpdated(content::RenderFrame* render_frame,
                          std::vector<mojom::ProductPtr> products) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnCartProductUpdated(std::move(products));
}

void OnVisitCheckout(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnVisitCheckout();
}

void OnPurchase(content::RenderFrame* render_frame) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnPurchase();
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

// The heuristics of cart pages are from top 30 US shopping domains.
// https://colab.corp.google.com/drive/1ANuCcRphLieSbhy5t05IEnOYLT5RmEdf#scrollTo=k9Sh9VvodKQx
const re2::RE2& GetVisitCartPatternAmazon() {
  static base::NoDestructor<re2::RE2> instance(
      "^/(-/[A-Za-z_-]+/)?gp/((.*/)?cart(/.*)?)(/|$)");
  return *instance;
}

const re2::RE2& GetVisitCartPatternApple() {
  static base::NoDestructor<re2::RE2> instance("/([^/]+/)?shop/([^/]+/)?bag$");
  return *instance;
}

const re2::RE2& GetVisitCartPatternMacy() {
  static base::NoDestructor<re2::RE2> instance("/(my-bag|bag(/[^/]+)*.ognc)$");
  return *instance;
}

const re2::RE2& GetVisitCartPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      "(/(my|co-|shopping[-_]?)?(cart|bag)(view)?(/|\\.|$|\\?))"
      "|"
      "(/checkout/([^/]+/)?(basket|bag)(/|\\.|$))"
      "|"
      "(/checkoutcart(display)?view(/|\\.|$))"
      "|"
      "(/bundles/shop(/|\\.|$))",
      options);
  return *instance;
}

// TODO(crbug/1164236): cover more shopping sites.
const re2::RE2& GetVisitCheckoutPattern() {
  static base::NoDestructor<re2::RE2> instance("/checkouts?(/|$)");
  return *instance;
}

// TODO(crbug/1164236): need i18n.
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

  const GURL url = request.Url();
  return url.DomainIs(host);
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

bool CommerceHintAgent::IsVisitCart(const GURL& url) {
  if (eTLDPlusOne(url) == kAmazonDomain) {
    return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                        GetVisitCartPatternAmazon()) ||
           url.path_piece() == "/gp/aw/c";
  }
  if (eTLDPlusOne(url) == kAppleDomain) {
    return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                        GetVisitCartPatternApple());
  }
  if (eTLDPlusOne(url) == kMacysDomain) {
    return PartialMatch(url.path_piece().substr(0, kLengthLimit),
                        GetVisitCartPatternMacy());
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

bool CommerceHintAgent::IsPurchase(base::StringPiece button_text) {
  return PartialMatch(button_text, GetPurchaseTextPattern());
}

std::string CommerceHintAgent::ExtractButtonText(
    const blink::WebFormElement& form) {
  static base::NoDestructor<WebString> kButton("button");

  const WebElementCollection& buttons = form.GetElementsByHTMLTagName(*kButton);

  std::vector<std::string> button_texts;
  for (WebElement button = buttons.FirstItem(); !button.IsNull();
       button = buttons.NextItem()) {
    // TODO(crbug/1164236): emulate innerText to be more robust.
    button_texts.push_back(base::UTF16ToUTF8(base::CollapseWhitespace(
        base::TrimWhitespace(button.TextContent().Utf16(),
                             base::TrimPositions::TRIM_ALL),
        true)));
  }
  return base::JoinString(button_texts, " ");
}

void CommerceHintAgent::ExtractProducts() {
  // TODO(crbug/1164236): Implement rate control.
  blink::WebLocalFrame* main_frame = render_frame()->GetWebFrame();

  std::string script =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
          IDR_CART_PRODUCT_EXTRACTION_JS);

  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
  blink::WebScriptSource source =
      blink::WebScriptSource(WebString::FromUTF8(script));

  JavaScriptRequest* request =
      new JavaScriptRequest(weak_factory_.GetWeakPtr());
  main_frame->RequestExecuteScriptInIsolatedWorld(
      ISOLATED_WORLD_ID_CHROME_INTERNAL, &source, 1, false,
      blink::WebLocalFrame::kAsynchronous, request);
}

CommerceHintAgent::JavaScriptRequest::JavaScriptRequest(
    base::WeakPtr<CommerceHintAgent> agent)
    : agent_(std::move(agent)) {}

CommerceHintAgent::JavaScriptRequest::~JavaScriptRequest() = default;

void CommerceHintAgent::JavaScriptRequest::Completed(
    const blink::WebVector<v8::Local<v8::Value>>& result) {
  if (!agent_)
    return;
  blink::WebLocalFrame* main_frame = agent_->render_frame()->GetWebFrame();
  if (result.empty() || result.begin()->IsEmpty())
    return;
  agent_->OnProductsExtracted(content::V8ValueConverter::Create()->FromV8Value(
      result[0], main_frame->MainWorldScriptContext()));
}

void CommerceHintAgent::OnProductsExtracted(
    std::unique_ptr<base::Value> results) {
  if (!results) {
    DLOG(ERROR) << "OnProductsExtracted() got empty results";
    return;
  }
  DVLOG(2) << "OnProductsExtracted: " << *results;
  // Don't update cart when the return value is not a list. This could be due to
  // that the cart is not loaded.
  if (!results->is_list())
    return;
  std::vector<mojom::ProductPtr> products;
  for (const auto& product : results->GetList()) {
    if (!product.is_dict())
      continue;
    const auto* image_url = product.FindKey("imageUrl");
    const auto* product_name = product.FindKey("title");
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->image_url = GURL(image_url->GetString());
    product_ptr->name = product_name->GetString();
    DVLOG(1) << "image_url = " << product_ptr->image_url;
    DVLOG(1) << "name = " << product_ptr->name;
    products.push_back(std::move(product_ptr));
  }
  OnCartProductUpdated(render_frame(), std::move(products));
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  DetectAddToCart(render_frame(), request);

  // TODO(crbug/1164236): use MutationObserver on cart instead.
  // Detect XHR in cart page.
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL& url(frame->GetDocument().Url());

  if (IsVisitCart(url) && IsSameDomainXHR(url.host(), request)) {
    DVLOG(1) << "In-cart XHR: " << request.Url();
    ExtractProducts();
  }
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  starting_url_ = url;
}

void CommerceHintAgent::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (!starting_url_.is_valid())
    return;
  if (IsAddToCart(starting_url_.PathForRequestPiece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame());
  }
  if (IsVisitCheckout(starting_url_)) {
    RecordCommerceEvent(CommerceEvent::kVisitCheckout);
    OnVisitCheckout(render_frame());
  }
  if (IsPurchase(starting_url_)) {
    RecordCommerceEvent(CommerceEvent::kPurchaseByURL);
    OnPurchase(render_frame());
  }

  starting_url_ = GURL();
}

void CommerceHintAgent::DidFinishLoad() {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL& url(frame->GetDocument().Url());

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

// TODO(crbug/1164236): use MutationObserver on cart instead.
void CommerceHintAgent::DidObserveLayoutShift(double score,
                                              bool after_input_or_scroll) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL url(frame->GetDocument().Url());

  if (IsVisitCart(url)) {
    DVLOG(1) << "In-cart layout shift: " << url;
    ExtractProducts();
  }
}
}  // namespace cart
