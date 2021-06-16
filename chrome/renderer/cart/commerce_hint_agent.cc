// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/cart/commerce_hints.mojom.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/renderer_resources.h"
#include "components/search/ntp_features.h"
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

constexpr base::FeatureParam<std::string> kSkipPattern{
    &ntp_features::kNtpChromeCartModule, "product-skip-pattern",
    // This regex does not match anything.
    "\\b\\B"};

std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsCartHeuristicsImprovementEnabled() {
  return base::GetFieldTrialParamByFeatureAsBool(
      ntp_features::kNtpChromeCartModule,
      "NtpChromeCartModuleHeuristicsImprovementParam", false);
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

// The heuristics of cart pages are from top 100 US shopping domains.
// https://colab.corp.google.com/drive/1fTGE_SQw_8OG4ubzQvWcBuyHEhlQ-pwQ?usp=sharing
// TODO(crbug.com/1189786): Using per-site pattern and full URL matching could
// be unnecessary. Improve this later by using general pattern if possible and
// more flexible matching.
const re2::RE2& GetVisitCartPattern(const GURL& url) {
  static base::NoDestructor<std::map<std::string, std::string>>
      heuristic_string_map([] {
        const base::StringPiece json_resource(
            ui::ResourceBundle::GetSharedInstance().GetRawDataResource(
                IDR_CART_DOMAIN_CART_URL_REGEX_JSON));
        const base::Value json(base::JSONReader::Read(json_resource).value());
        DCHECK(json.is_dict());
        std::map<std::string, std::string> map;
        for (const auto& item : json.DictItems()) {
          map.insert(
              {std::move(item.first), std::move(item.second.GetString())});
        }
        return map;
      }());
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      heuristic_regex_map;
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  const std::string& domain = eTLDPlusOne(url);
  if (heuristic_string_map->find(domain) == heuristic_string_map->end()) {
    // clang-format off
    static base::NoDestructor<re2::RE2> instance(
        "(^https?://cart\\.)"
        "|"
        "(/("
          "(((my|co|shopping)[-_]?)?(cart|bag)(view|display)?)"
          "|"
          "(checkout/([^/]+/)?(basket|bag))"
          "|"
          "(checkoutcart(display)?view)"
          "|"
          "(bundles/shop)"
          "|"
          "((ajax)?orderitemdisplay(view)?)"
          "|"
          "(cart-show)"
        ")(/|\\.|$))",
        options);
    // clang-format on
    return *instance;
  }
  if (heuristic_regex_map->find(domain) == heuristic_regex_map->end()) {
    heuristic_regex_map->insert(
        {domain, std::make_unique<re2::RE2>(heuristic_string_map->at(domain),
                                            options)});
  }
  return *heuristic_regex_map->at(domain);
}

// TODO(crbug/1164236): cover more shopping sites.
const re2::RE2& GetVisitCheckoutPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // clang-format off
  static base::NoDestructor<re2::RE2> instance(
      "/("
      "("
        "("
          "(begin|billing|cart|payment|start|review|final|order|secure|new)"
          "[-_]?"
        ")?"
        "(checkout|chkout)(s)?"
        "([-_]?(begin|billing|cart|payment|start|review))?"
      ")"
      "|"
      "(\\w+(checkout|chkout)(s)?)"
      ")(/|\\.|$|\\?)",
      options);
  // clang-format on
  return *instance;
}

const re2::RE2& GetSkipPattern() {
  static base::NoDestructor<re2::RE2> instance([] {
    const std::string& pattern = kSkipPattern.Get();
    DVLOG(1) << "SkipPattern = " << pattern;
    return pattern;
  }());
  return *instance;
}

// TODO(crbug/1164236): need i18n.
const re2::RE2& GetPurchaseTextPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // clang-format off
  static base::NoDestructor<re2::RE2> instance(
      "^("
      "("
        "(place|submit|complete|confirm|finalize|make)(\\s(an|your|my|this))?"
        "(\\ssecure)?\\s(order|purchase|checkout|payment)"
      ")"
      "|"
      "((pay|buy)(\\ssecurely)?(\\sUSD)?\\s(it|now|((\\$)?\\d+(\\.\\d+)?)))"
      "|"
      "((make|authorise|authorize|secure)\\spayment)"
      "|"
      "(confirm\\s(and|&)\\s(buy|purchase|order|pay|checkout))"
      "|"
      "((\\W)*(buy|purchase|order|pay|checkout)(\\W)*)"
      ")$",
      options);
  // clang-format on
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
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  const GURL& navigation_url(frame->GetDocument().Url());

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

  // Per-site hard-coded exclusion rules:
  if (navigation_url.DomainIs("costco.com") && url.DomainIs("clicktale.net"))
    return;
  if (navigation_url.DomainIs("lululemon.com") &&
      url.DomainIs("launchdarkly.com"))
    return;
  if (navigation_url.DomainIs("qvc.com"))
    return;
  if (navigation_url.DomainIs("hsn.com") && url.DomainIs("granify.com"))
    return;

  if (IsCartHeuristicsImprovementEnabled()) {
    if (navigation_url.DomainIs("abebooks.com"))
      return;
    if (navigation_url.DomainIs("abercrombie.com"))
      return;
    if (navigation_url.DomainIs(kAmazonDomain) &&
        url.host() != "fls-na.amazon.com")
      return;
    if (navigation_url.DomainIs("bestbuy.com"))
      return;
    if (navigation_url.DomainIs("containerstore.com"))
      return;
    if (navigation_url.DomainIs("gap.com") && url.DomainIs("granify.com"))
      return;
    if (navigation_url.DomainIs("kohls.com"))
      return;
    if (navigation_url.DomainIs("officedepot.com") &&
        url.DomainIs("chatid.com"))
      return;
    if (navigation_url.DomainIs("pier1.com"))
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

    // Per-site hard-coded exclusion rules:
    if (navigation_url.DomainIs("groupon.com") && buf.size() > 10000)
      return;

    if (CommerceHintAgent::IsAddToCart(str)) {
      RecordCommerceEvent(CommerceEvent::kAddToCartByForm);
      DVLOG(2) << "Matched add-to-cart. Request from \"" << navigation_url
               << "\" to \"" << url << "\" with payload (size = " << str.size()
               << ") \"" << str << "\"";
      OnAddToCart(render_frame);
      return;
    }
  }
}

std::string CanonicalURL(const GURL& url) {
  return base::JoinString({url.scheme_piece(), "://", url.host_piece(),
                           url.path_piece().substr(0, kLengthLimit)},
                          "");
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
  return PartialMatch(CanonicalURL(url).substr(0, kLengthLimit),
                      GetVisitCartPattern(url));
}

bool CommerceHintAgent::IsVisitCheckout(const GURL& url) {
  if (url.DomainIs(kAmazonDomain)) {
    return base::StartsWith(url.path_piece(), "/gp/buy/spc/handlers/display");
  }
  if (url.DomainIs(kEbayDomain)) {
    return url.spec().find("pay.ebay.com/rgxo") != std::string::npos;
  }
  return PartialMatch(CanonicalURL(url).substr(0, kLengthLimit),
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

bool CommerceHintAgent::ShouldSkip(base::StringPiece product_name) {
  return PartialMatch(product_name.substr(0, kLengthLimit), GetSkipPattern());
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
    if (ShouldSkip(product_ptr->name)) {
      DVLOG(1) << "skipped";
      continue;
    }
    products.push_back(std::move(product_ptr));
  }
  OnCartProductUpdated(render_frame(), std::move(products));
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL& url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  DetectAddToCart(render_frame(), request);

  // TODO(crbug/1164236): use MutationObserver on cart instead.
  // Detect XHR in cart page.
  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  if (!url.SchemeIs(url::kHttpsScheme))
    return;
  if (IsVisitCart(url) && IsSameDomainXHR(url.host(), request)) {
    DVLOG(1) << "In-cart XHR: " << request.Url();
    ExtractProducts();
  }
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    base::Optional<blink::WebNavigationType> navigation_type) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
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
  if (!url.SchemeIs(url::kHttpsScheme))
    return;

  if (IsVisitCart(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCart);
    OnVisitCart(render_frame());
    ExtractProducts();
  }
}

void CommerceHintAgent::WillSubmitForm(const blink::WebFormElement& form) {
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

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
  if (!url.SchemeIs(url::kHttpsScheme))
    return;

  if (IsVisitCart(url)) {
    DVLOG(1) << "In-cart layout shift: " << url;
    ExtractProducts();
  }
}
}  // namespace cart
