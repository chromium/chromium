// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/renderer/cart/commerce_hint_agent.h"

#include <string_view>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "chrome/grit/renderer_resources.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/commerce_heuristics_data.h"
#include "components/commerce/core/commerce_heuristics_data_metrics_helper.h"
#include "components/commerce/core/heuristics/commerce_heuristics_provider.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/common/loader/http_body_element_type.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_agent_group_scheduler.h"
#include "third_party/blink/public/platform/web_http_body.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_element_collection.h"
#include "third_party/blink/public/web/web_form_element.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8-isolate.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/search/ntp_features.h"
#endif

using base::UserMetricsAction;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebString;

namespace cart {

namespace {

constexpr unsigned kLengthLimit = 4096;
constexpr char kAmazonDomain[] = "amazon.com";
constexpr char kEbayDomain[] = "ebay.com";
constexpr char kElectronicExpressDomain[] = "electronicexpress.com";
constexpr char kGStoreHost[] = "store.google.com";
constexpr char kInputType[] = "INPUT";
constexpr char kValueAttributeName[] = "value";

constexpr base::FeatureParam<std::string> kSkipPattern{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "product-skip-pattern",
#else
  &commerce::kCommerceHintAndroid, "product-skip-pattern",
#endif
      // This regex does not match anything.
      "\\b\\B"
};

// This is based on top 30 US shopping sites.
// TODO(crbug.com/40163450): cover more shopping sites.
constexpr base::FeatureParam<std::string> kAddToCartPattern{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "add-to-cart-pattern",
#else
  &commerce::kCommerceHintAndroid, "add-to-cart-pattern",
#endif
      "(\\b|[^a-z])"
      "((add(ed)?(-|_|(%20)|\\s)?(item)?(-|_|(%20)|\\s)?to(-|_|(%20)|\\s)?("
      "cart|"
      "basket|bag)"
      ")|(cart\\/add)|(checkout\\/basket)|(cart_type)|(isquickaddtocartbutton))"
      "(\\b|[^a-z])"
};

constexpr base::FeatureParam<std::string> kSkipAddToCartMapping{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "skip-add-to-cart-mapping",
#else
  &commerce::kCommerceHintAndroid, "skip-add-to-cart-mapping",
#endif
      // Empty JSON string.
      ""
};

constexpr base::FeatureParam<std::string> kPurchaseURLPatternMapping{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "purchase-url-pattern-mapping",
#else
  &commerce::kCommerceHintAndroid, "purchase-url-pattern-mapping",
#endif
      // Empty JSON string.
      ""
};

constexpr base::FeatureParam<std::string> kPurchaseButtonPattern{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "purchase-button-pattern",
#else
  &commerce::kCommerceHintAndroid, "purchase-button-pattern",
#endif
      // clang-format off
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
    ")$"
  // clang-format on
};

constexpr base::FeatureParam<std::string> kPurchaseButtonPatternMapping{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "purchase-button-pattern-mapping",
#else
  &commerce::kCommerceHintAndroid, "purchase-button-pattern-mapping",
#endif
      // Empty JSON map.
      "{}"
};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionGapTime{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "cart-extraction-gap-time",
#else
  &commerce::kCommerceHintAndroid, "cart-extraction-gap-time",
#endif
      base::Seconds(2)
};

constexpr base::FeatureParam<int> kCartExtractionMaxCount{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "cart-extraction-max-count",
#else
  &commerce::kCommerceHintAndroid, "cart-extraction-max-count",
#endif
      20
};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionMinTaskTime{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "cart-extraction-min-task-time",
#else
  &commerce::kCommerceHintAndroid, "cart-extraction-min-task-time",
#endif
      base::Seconds(0.01)
};

constexpr base::FeatureParam<double> kCartExtractionDutyCycle{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "cart-extraction-duty-cycle",
#else
  &commerce::kCommerceHintAndroid, "cart-extraction-duty-cycle",
#endif
      0.05
};

constexpr base::FeatureParam<base::TimeDelta> kCartExtractionTimeout{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "cart-extraction-timeout",
#else
  &commerce::kCommerceHintAndroid, "cart-extraction-timeout",
#endif
      base::Seconds(0.25)
};

constexpr base::FeatureParam<std::string> kProductIdPatternMapping{
#if !BUILDFLAG(IS_ANDROID)
  &ntp_features::kNtpChromeCartModule, "product-id-pattern-mapping",
#else
  &commerce::kCommerceHintAndroid, "product-id-pattern-mapping",
#endif
      // Empty JSON string.
      ""
};

constexpr base::FeatureParam<std::string> kCouponProductIdPatternMapping{
    &commerce::kRetailCoupons, "coupon-product-id-pattern-mapping",
    // Empty JSON string.
    ""};

constexpr base::FeatureParam<std::string> kDOMBasedAddToCartRequestPattern{
    &commerce::kChromeCartDomBasedHeuristics, "add-to-cart-request-pattern",
    "(cart|product)?[\"|_|-]quantity\":[\"]?[\\d]+"};

std::string eTLDPlusOne(const GURL& url) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

bool IsCartHeuristicsImprovementEnabled() {
#if !BUILDFLAG(IS_ANDROID)
  return base::GetFieldTrialParamByFeatureAsBool(
      ntp_features::kNtpChromeCartModule,
      ntp_features::kNtpChromeCartModuleHeuristicsImprovementParam, true);
#else
  return base::GetFieldTrialParamByFeatureAsBool(
      commerce::kCommerceHintAndroid,
      commerce::kCommerceHintAndroidHeuristicsImprovementParam, true);
#endif
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
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.AddToCartByPOST", true);
      DVLOG(1) << "Commerce.AddToCart by POST form";
      base::RecordAction(base::UserMetricsAction("Commerce.AddToCart"));
      break;
    case CommerceEvent::kAddToCartByURL:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.AddToCartByURL", true);
      DVLOG(1) << "Commerce.AddToCart by URL";
      base::RecordAction(base::UserMetricsAction("Commerce.AddToCart"));
      break;
    case CommerceEvent::kVisitCart:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.VisitCart", true);
      DVLOG(1) << "Commerce.VisitCart";
      base::RecordAction(base::UserMetricsAction("Commerce.VisitCart"));
      break;
    case CommerceEvent::kVisitCheckout:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.VisitCheckout", true);
      DVLOG(1) << "Commerce.VisitCheckout";
      base::RecordAction(base::UserMetricsAction("Commerce.VisitCheckout"));
      break;
    case CommerceEvent::kPurchaseByForm:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.PurchaseByPOST", true);
      DVLOG(1) << "Commerce.Purchase by POST form";
      base::RecordAction(base::UserMetricsAction("Commerce.Purchase"));
      break;
    case CommerceEvent::kPurchaseByURL:
      LOCAL_HISTOGRAM_BOOLEAN("Commerce.Carts.PurchaseByURL", true);
      DVLOG(1) << "Commerce.Purchase by URL";
      base::RecordAction(base::UserMetricsAction("Commerce.Purchase"));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

mojo::Remote<mojom::CommerceHintObserver> GetObserver(
    content::RenderFrame* render_frame) {
  // Connect to Mojo service on browser to notify commerce signals.
  mojo::Remote<mojom::CommerceHintObserver> observer;

  // Subframes including fenced frames shouldn't be reached here.
  DCHECK(render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree());

  render_frame->GetBrowserInterfaceBroker().GetInterface(
      observer.BindNewPipeAndPassReceiver());
  return observer;
}

std::optional<GURL> ScanCartURL(content::RenderFrame* render_frame) {
  blink::WebDocument doc = render_frame->GetWebFrame()->GetDocument();

  std::optional<GURL> best;
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

void OnAddToCart(content::RenderFrame* render_frame,
                 const std::string& product_id = std::string()) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnAddToCart(ScanCartURL(render_frame), product_id);
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

void OnFormSubmit(content::RenderFrame* render_frame, bool is_purchase) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnFormSubmit(is_purchase);
}

void OnWillSendRequest(content::RenderFrame* render_frame, bool is_addtocart) {
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame);
  observer->OnWillSendRequest(is_addtocart);
}

const re2::RE2& GetAddToCartPattern() {
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetAddToCartRequestPattern();
  if (pattern_from_component &&
      kAddToCartPattern.Get() == kAddToCartPattern.default_value) {
    return *pattern_from_component;
  }
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kAddToCartPattern.Get(),
                                               options);
  return *instance;
}

const re2::RE2& GetDOMBasedAddToCartPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(
      kDOMBasedAddToCartRequestPattern.Get(), options);
  return *instance;
}

const std::map<std::string, std::string>& GetPurchaseURLPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    base::Value json(
        base::JSONReader::Read(
            kPurchaseURLPatternMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance()
                      .LoadDataResourceString(
                          IDR_PURCHASE_URL_REGEX_DOMAIN_MAPPING_JSON)
                : kPurchaseURLPatternMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto&& item : json.GetDict()) {
      map.insert({std::move(item.first), std::move(item.second).TakeString()});
    }
    return map;
  }());
  return *pattern_map;
}

const std::map<std::string, std::string>& GetPurchaseButtonPatternMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> pattern_map([] {
    base::Value json(
        base::JSONReader::Read(kPurchaseButtonPatternMapping.Get()).value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto&& item : json.GetDict()) {
      map.insert({std::move(item.first), std::move(item.second).TakeString()});
    }
    return map;
  }());
  return *pattern_map;
}

const re2::RE2* GetVisitPurchasePattern(const GURL& url) {
  std::string domain = eTLDPlusOne(url);
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetPurchasePageURLPatternForDomain(domain);
  if (pattern_from_component && kPurchaseURLPatternMapping.Get() ==
                                    kPurchaseURLPatternMapping.default_value) {
    return pattern_from_component;
  }
  const std::map<std::string, std::string>& purchase_string_map =
      GetPurchaseURLPatternMapping();
  if (purchase_string_map.find(domain) == purchase_string_map.end()) {
    return nullptr;
  }
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      purchase_regex_map;
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (purchase_regex_map->find(domain) == purchase_regex_map->end()) {
    purchase_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(purchase_string_map.at(domain), options)});
  }
  return purchase_regex_map->at(domain).get();
}

const re2::RE2& GetSkipPattern() {
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetProductSkipPattern();
  if (pattern_from_component &&
      kSkipPattern.Get() == kSkipPattern.default_value) {
    DVLOG(1) << "SkipPattern = " << pattern_from_component->pattern();
    CommerceHeuristicsDataMetricsHelper::RecordSkipProductPatternSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT);
    return *pattern_from_component;
  }
  static base::NoDestructor<re2::RE2> instance([] {
    const std::string& pattern = kSkipPattern.Get();
    DVLOG(1) << "SkipPattern = " << pattern;
    return pattern;
  }());
  CommerceHeuristicsDataMetricsHelper::RecordSkipProductPatternSource(
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER);
  return *instance;
}

// TODO(crbug.com/40163450): need i18n.
const re2::RE2& GetPurchaseTextPattern() {
  auto* pattern_from_component =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetPurchaseButtonTextPattern();
  if (pattern_from_component &&
      kPurchaseButtonPattern.Get() == kPurchaseButtonPattern.default_value) {
    return *pattern_from_component;
  }
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kPurchaseButtonPattern.Get(),
                                               options);
  return *instance;
}

bool GetProductIdFromRequest(std::string_view request,
                             std::string* product_id) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> re("(product_id|pr1id)=(\\w+)", options);
  return RE2::PartialMatch(request, *re, nullptr, product_id);
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

const std::map<std::string, std::string>& GetSkipAddToCartMapping() {
  static base::NoDestructor<std::map<std::string, std::string>> skip_map([] {
    base::Value json(
        base::JSONReader::Read(
            kSkipAddToCartMapping.Get().empty()
                ? ui::ResourceBundle::GetSharedInstance()
                      .LoadDataResourceString(
                          IDR_SKIP_ADD_TO_CART_REQUEST_DOMAIN_MAPPING_JSON)
                : kSkipAddToCartMapping.Get())
            .value());
    DCHECK(json.is_dict());
    std::map<std::string, std::string> map;
    for (auto&& item : json.GetDict()) {
      map.insert({std::move(item.first), std::move(item.second).TakeString()});
    }
    return map;
  }());
  return *skip_map;
}

bool DetectAddToCart(content::RenderFrame* render_frame,
                     const blink::WebURLRequest& request,
                     bool should_use_dom_based_heuristics) {
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  const GURL& navigation_url(frame->GetDocument().Url());
  const GURL& url = request.Url();

  if (CommerceHintAgent::ShouldSkipAddToCartRequest(navigation_url, url)) {
    return false;
  }
  bool is_add_to_cart = false;
  if (navigation_url.DomainIs("dickssportinggoods.com")) {
    is_add_to_cart = CommerceHintAgent::IsAddToCart(url.spec());
  } else if (url.DomainIs("rei.com")) {
    // TODO(crbug.com/40754689): There are other true positives like
    // 'neo-product/rs/cart/item' that are missed here. Figure out a more
    // comprehensive solution.
    is_add_to_cart = url.path_piece() == "/rest/cart/item";
  } else if (navigation_url.DomainIs(kElectronicExpressDomain)) {
    is_add_to_cart =
        CommerceHintAgent::IsAddToCart(url.spec()) &&
        GetProductIdFromRequest(url.spec().substr(0, kLengthLimit), nullptr);
  } else if (navigation_url.host() == kGStoreHost) {
    is_add_to_cart = url.spec().find("O2JPA") != std::string::npos;
  } else if (url.DomainIs("zappos.com")) {
    is_add_to_cart = url.spec().find("mobileapi/v1/cart?displayRewards=true") !=
                     std::string::npos;
  } else {
    is_add_to_cart = CommerceHintAgent::IsAddToCart(url.path_piece());
  }
  if (is_add_to_cart) {
    std::string url_product_id;
    if (commerce::IsPartnerMerchant(navigation_url)) {
      GetProductIdFromRequest(url.spec().substr(0, kLengthLimit),
                              &url_product_id);
    }
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame, std::move(url_product_id));
    return true;
  }

  if (IsCartHeuristicsImprovementEnabled()) {
    if (navigation_url.DomainIs("abebooks.com"))
      return false;
    if (navigation_url.DomainIs("abercrombie.com"))
      return false;
    if (navigation_url.DomainIs(kAmazonDomain) &&
        url.host() != "fls-na.amazon.com")
      return false;
    if (navigation_url.DomainIs("bestbuy.com"))
      return false;
    if (navigation_url.DomainIs("containerstore.com"))
      return false;
    if (navigation_url.DomainIs("gap.com") && url.DomainIs("granify.com"))
      return false;
    if (navigation_url.DomainIs("kohls.com"))
      return false;
    if (navigation_url.DomainIs("officedepot.com") &&
        url.DomainIs("chatid.com"))
      return false;
    if (navigation_url.DomainIs("pier1.com"))
      return false;
  }

  blink::WebHTTPBody body = request.HttpBody();
  if (body.IsNull())
    return false;

  unsigned i = 0;
  blink::WebHTTPBody::Element element;
  while (body.ElementAt(i++, element)) {
    if (element.type != blink::HTTPBodyElementType::kTypeData)
      continue;

    // TODO(crbug.com/40165127): this copy is avoidable if element is guaranteed
    // to have contiguous buffer.
    std::vector<uint8_t> buf = element.data.Copy().ReleaseVector();
    std::string_view str(reinterpret_cast<char*>(buf.data()), buf.size());

    // Per-site hard-coded exclusion rules:
    if (navigation_url.DomainIs("groupon.com") && buf.size() > 10000)
      return false;

    // Per-site skipping length limit when checking request text.
    bool skip_length_limit = navigation_url.DomainIs("otterbox.com");

    bool is_add_to_cart_request =
        CommerceHintAgent::IsAddToCart(str, skip_length_limit);
    if (should_use_dom_based_heuristics && !is_add_to_cart_request) {
      is_add_to_cart_request =
          CommerceHintAgent::IsAddToCartForDomBasedHeuristics(str);
    }

    if (is_add_to_cart_request) {
      std::string product_id;
      if (commerce::IsPartnerMerchant(url)) {
        GetProductIdFromRequest(str.substr(0, kLengthLimit), &product_id);
      }
      RecordCommerceEvent(CommerceEvent::kAddToCartByForm);
      DVLOG(2) << "Matched add-to-cart. Request from \"" << navigation_url
               << "\" to \"" << url << "\" with payload (size = " << str.size()
               << ") \"" << str << "\"";
      OnAddToCart(render_frame, std::move(product_id));
      return true;
    }
  }
  return false;
}

std::string CanonicalURL(const GURL& url) {
  return base::JoinString({url.scheme_piece(), "://", url.host_piece(),
                           url.path_piece().substr(0, kLengthLimit)},
                          "");
}

const WebString GetProductExtractionScript(
    const std::string& product_id_json_component,
    const std::string& cart_extraction_script_component) {
  std::string script_string;
  if (cart_extraction_script_component.empty()) {
    script_string =
        ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
            IDR_CART_PRODUCT_EXTRACTION_JS);
    if (IsCartHeuristicsImprovementEnabled()) {
      script_string = "var isImprovementEnabled = true;\n" + script_string;
    }
    const std::string config =
        "var kSleeperMinTaskTimeMs = " +
        base::NumberToString(
            kCartExtractionMinTaskTime.Get().InMillisecondsF()) +
        ";\n" + "var kSleeperDutyCycle = " +
        base::NumberToString(kCartExtractionDutyCycle.Get()) + ";\n" +
        "var kTimeoutMs = " +
        base::NumberToString(kCartExtractionTimeout.Get().InMillisecondsF()) +
        ";\n";
    DVLOG(2) << config;
    script_string = config + script_string;
    CommerceHeuristicsDataMetricsHelper::RecordCartExtractionScriptSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_RESOURCE);
  } else {
    script_string = cart_extraction_script_component;
    CommerceHeuristicsDataMetricsHelper::RecordCartExtractionScriptSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT);
  }

  if (!product_id_json_component.empty()) {
    script_string = "var idExtractionMap = " + product_id_json_component +
                    ";\n" + script_string;
    CommerceHeuristicsDataMetricsHelper::RecordProductIDExtractionPatternSource(
        CommerceHeuristicsDataMetricsHelper::HeuristicsSource::FROM_COMPONENT);
    return WebString::FromUTF8(std::move(script_string));
  }
  CommerceHeuristicsDataMetricsHelper::RecordProductIDExtractionPatternSource(
      CommerceHeuristicsDataMetricsHelper::HeuristicsSource::
          FROM_FEATURE_PARAMETER);
  const std::string id_extraction_map =
      kProductIdPatternMapping.Get().empty()
          ? ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
                IDR_CART_DOMAIN_PRODUCT_ID_REGEX_JSON)
          : kProductIdPatternMapping.Get();
  script_string =
      "var idExtractionMap = " + id_extraction_map + ";\n" + script_string;

  const std::string coupon_id_extraction_map =
      kCouponProductIdPatternMapping.Get();
  if (!coupon_id_extraction_map.empty()) {
    script_string = "var couponIdExtractionMap = " + coupon_id_extraction_map +
                    ";\n" + script_string;
  }
  return WebString::FromUTF8(std::move(script_string));
}

}  // namespace

using commerce_heuristics::CommerceHeuristicsData;

CommerceHintAgent::CommerceHintAgent(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<CommerceHintAgent>(render_frame) {
  DCHECK(render_frame);

  // Subframes including fenced frames shouldn't be reached here.
  DCHECK(render_frame->IsMainFrame() && !render_frame->IsInFencedFrameTree());

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;

  content::RenderThread::Get()->BindHostReceiver(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);
}

CommerceHintAgent::~CommerceHintAgent() = default;

bool CommerceHintAgent::IsAddToCart(std::string_view str,
                                    bool skip_length_limit) {
  return RE2::PartialMatch(
      skip_length_limit ? str : str.substr(0, kLengthLimit),
      GetAddToCartPattern());
}

bool CommerceHintAgent::IsAddToCartForDomBasedHeuristics(std::string_view str) {
  return RE2::PartialMatch(str.substr(0, kLengthLimit),
                           GetDOMBasedAddToCartPattern());
}

// TODO(crbug.com/40219864): Remove below two APIs and move all related unit
// tests to component.
bool CommerceHintAgent::IsVisitCart(const GURL& url) {
  return commerce_heuristics::IsVisitCart(url);
}

bool CommerceHintAgent::IsVisitCheckout(const GURL& url) {
  return commerce_heuristics::IsVisitCheckout(url);
}

bool CommerceHintAgent::IsPurchase(const GURL& url) {
  auto* pattern = GetVisitPurchasePattern(url);
  if (!pattern)
    return false;
  return RE2::PartialMatch(CanonicalURL(url).substr(0, kLengthLimit), *pattern);
}

bool CommerceHintAgent::IsPurchase(const GURL& url,
                                   std::string_view button_text) {
  const std::map<std::string, std::string>& purchase_string_map =
      GetPurchaseButtonPatternMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      purchase_regex_map;
  std::string domain = eTLDPlusOne(url);
  if (purchase_string_map.find(domain) == purchase_string_map.end()) {
    return RE2::PartialMatch(button_text, GetPurchaseTextPattern());
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (purchase_regex_map->find(domain) == purchase_regex_map->end()) {
    purchase_regex_map->insert(
        {domain,
         std::make_unique<re2::RE2>(purchase_string_map.at(domain), options)});
  }
  return RE2::PartialMatch(button_text, *purchase_regex_map->at(domain));
}

bool CommerceHintAgent::ShouldSkip(std::string_view product_name) {
  return RE2::PartialMatch(product_name.substr(0, kLengthLimit),
                           GetSkipPattern());
}

const std::vector<std::string> CommerceHintAgent::ExtractButtonTexts(
    const blink::WebFormElement& form) {
  static base::NoDestructor<WebString> kButton("button");

  const WebElementCollection& buttons = form.GetElementsByHTMLTagName(*kButton);

  std::vector<std::string> button_texts;
  for (WebElement button = buttons.FirstItem(); !button.IsNull();
       button = buttons.NextItem()) {
    // TODO(crbug.com/40163450): emulate innerText to be more robust.
    button_texts.push_back(base::UTF16ToUTF8(base::CollapseWhitespace(
        base::TrimWhitespace(button.TextContent().Utf16(),
                             base::TrimPositions::TRIM_ALL),
        true)));
  }
  return button_texts;
}

bool CommerceHintAgent::IsAddToCartButton(blink::WebElement& element) {
  // Find the first non-null, non-empty element and terminates anytime an
  // element with wrong size is found.
  std::string button_text;
  std::u16string button_text_utf16;
  while (!element.IsNull()) {
    gfx::Size client_size = element.GetClientSize();
    if (!commerce_heuristics::IsAddToCartButtonSpec(client_size.height(),
                                                    client_size.width())) {
      return false;
    }
    base::TrimWhitespace(element.TextContent().Utf16(), base::TRIM_ALL,
                         &button_text_utf16);
    button_text = base::UTF16ToUTF8(button_text_utf16);
    if (button_text.empty() && element.TagName().Ascii() == kInputType &&
        !element.GetAttribute(kValueAttributeName).IsEmpty()) {
      base::TrimWhitespace(element.GetAttribute(kValueAttributeName).Utf16(),
                           base::TRIM_ALL, &button_text_utf16);
      button_text = base::UTF16ToUTF8(button_text_utf16);
    }
    if (!button_text.empty())
      break;
    if (!element.ParentNode().IsElementNode()) {
      return false;
    }
    element = element.ParentNode().To<blink::WebElement>();
  }
  if (element.IsNull() ||
      !commerce_heuristics::IsAddToCartButtonTag(element.TagName().Ascii()) ||
      !commerce_heuristics::IsAddToCartButtonText(button_text)) {
    return false;
  }
  return true;
}

void CommerceHintAgent::MaybeExtractProducts() {
  // TODO(crbug.com/40194728): Add a test for rate control based on whether the
  // histogram is recorded.
  if (is_extraction_pending_) {
    DVLOG(1) << "Extraction is scheduled. Skip this request.";
    return;
  }
  if (extraction_count_ >= kCartExtractionMaxCount.Get()) {
    DVLOG(1) << "Extraction exceeds quota. Skip until navigation.";
    return;
  }
  is_extraction_pending_ = true;
  DVLOG(1) << "Scheduled extraction";
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CommerceHintAgent::ExtractProducts,
                     weak_factory_.GetWeakPtr()),
      kCartExtractionGapTime.Get());
}

void CommerceHintAgent::ExtractProducts() {
  if (!IsVisitCart(GURL(render_frame()->GetWebFrame()->GetDocument().Url()))) {
    return;
  }
  is_extraction_pending_ = false;
  if (is_extraction_running_) {
    DVLOG(1) << "Extraction is running. Try again later.";
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CommerceHintAgent::MaybeExtractProducts,
                       weak_factory_.GetWeakPtr()),
        kCartExtractionGapTime.Get());
    return;
  }
  // Use current script if it has already been initialized; otherwise fetch
  // script from browser side.
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame());
  auto* observer_ptr = observer.get();
  observer_ptr->OnCartExtraction(
      base::BindOnce(&CommerceHintAgent::ExtractCartWithUpdatedScript,
                     weak_factory_.GetWeakPtr(), std::move(observer)));
}

void CommerceHintAgent::ExtractCartWithUpdatedScript(
    mojo::Remote<mojom::CommerceHintObserver> observer,
    const std::string& product_id_json,
    const std::string& cart_extraction_script) {
  is_extraction_running_ = true;
  DVLOG(2) << "is_extraction_running_ = " << is_extraction_running_;

  blink::WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  v8::HandleScope handle_scope(main_frame->GetAgentGroupScheduler()->Isolate());
  blink::WebScriptSource source = blink::WebScriptSource(
      GetProductExtractionScript(product_id_json, cart_extraction_script));

  main_frame->RequestExecuteScript(
      ISOLATED_WORLD_ID_CHROME_INTERNAL, base::span_from_ref(source),
      blink::mojom::UserActivationOption::kDoNotActivate,
      blink::mojom::EvaluationTiming::kAsynchronous,
      blink::mojom::LoadEventBlockingOption::kDoNotBlock,
      base::BindOnce(&CommerceHintAgent::OnProductsExtracted,
                     weak_factory_.GetWeakPtr()),
      blink::BackForwardCacheAware::kAllow,
      blink::mojom::WantResultOption::kWantResult,
      blink::mojom::PromiseResultOption::kAwait);
}

void CommerceHintAgent::OnProductsExtracted(std::optional<base::Value> results,
                                            base::TimeTicks start_time) {
  // Only record when the start time is correctly captured.
  if (!results || !results->is_dict())
    return;
  base::Value::Dict& results_dict = results->GetDict();
  if (!start_time.is_null()) {
    results_dict.Set("execution_ms",
                     (base::TimeTicks::Now() - start_time).InMillisecondsF());
  }

  DVLOG(2) << "OnProductsExtracted: " << *results;

  auto builder = ukm::builders::Shopping_CartExtraction(
      render_frame()->GetWebFrame()->GetDocument().GetUkmSourceId());
  auto record_time = [&](const std::string& key,
                         const std::string& metric_name) {
    std::optional<double> optional_time = results_dict.FindDouble(key);
    if (!optional_time) {
      return;
    }

    double time_value = optional_time.value();
    base::TimeDelta time = base::Milliseconds(time_value);
    base::UmaHistogramTimes("Commerce.Carts." + metric_name, time);

    if (metric_name == "ExtractionLongestTaskTime") {
      builder.SetExtractionLongestTaskTime(time_value);
    } else if (metric_name == "ExtractionTotalTasksTime") {
      builder.SetExtractionTotalTasksTime(time_value);
    } else if (metric_name == "ExtractionElapsedTime") {
      builder.SetExtractionElapsedTime(time_value);
    } else if (metric_name == "ExtractionExecutionTime") {
      builder.SetExtractionExecutionTime(time_value);
    }
  };
  record_time("longest_task_ms", "ExtractionLongestTaskTime");
  record_time("total_tasks_ms", "ExtractionTotalTasksTime");
  record_time("elapsed_ms", "ExtractionElapsedTime");
  record_time("execution_ms", "ExtractionExecutionTime");

  std::optional<bool> timedout = results_dict.FindBool("timedout");
  if (timedout) {
    base::UmaHistogramBoolean("Commerce.Carts.ExtractionTimedOut",
                              timedout.value());
    builder.SetExtractionTimedOut(timedout.value());
  }
  builder.Record(ukm_recorder_.get());

  const base::Value::List* extracted_products =
      results_dict.FindList("products");
  // Don't update cart when the return value does not exist or is not a list.
  // This could be due to that the cart is not loaded.
  if (!extracted_products) {
    return;
  }
  std::vector<mojom::ProductPtr> products;
  for (const auto& product_val : *extracted_products) {
    if (!product_val.is_dict()) {
      continue;
    }

    const base::Value::Dict& product = product_val.GetDict();
    const std::string* image_url = product.FindString("imageUrl");
    const std::string* product_name = product.FindString("title");
    mojom::ProductPtr product_ptr(mojom::Product::New());
    product_ptr->image_url = GURL(*image_url);
    product_ptr->name = *product_name;
    DVLOG(1) << "image_url = " << product_ptr->image_url;
    DVLOG(1) << "name = " << product_ptr->name;
    if (ShouldSkip(product_ptr->name)) {
      DVLOG(1) << "skipped";
      continue;
    }
    const std::string* product_id = product.FindString("productId");
    if (product_id) {
      DVLOG(1) << "product_id = " << *product_id;
      DCHECK(!product_id->empty());
      product_ptr->product_id = *product_id;
    }
    products.push_back(std::move(product_ptr));
  }
  OnCartProductUpdated(render_frame(), std::move(products));

  is_extraction_running_ = false;
  extraction_count_++;
  DVLOG(2) << "is_extraction_running_ = " << is_extraction_running_;
}

bool CommerceHintAgent::ShouldUseDOMBasedHeuristics() {
  if (!should_use_dom_heuristics_.has_value()) {
    const GURL& url(render_frame()->GetWebFrame()->GetDocument().Url());
    should_use_dom_heuristics_ =
        commerce_heuristics::ShouldUseDOMBasedHeuristics(url);
  }
  return should_use_dom_heuristics_.value();
}

void CommerceHintAgent::OnDestruct() {
  delete this;
}

void CommerceHintAgent::WillSendRequest(const blink::WebURLRequest& request) {
  if (!should_skip_.has_value() || should_skip_.value()) {
    return;
  }
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL& url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  // The rest of this method is not concerned with data URLs but makes a copy of
  // the URL which can be expensive for large data URLs.
  // TODO(crbug.com/40224104): Clean up this method to avoid copies.
  if (request.Url().ProtocolIs(url::kDataScheme)) {
    return;
  }

  // Only check XHR POST requests for add-to-cart.
  // Other add-to-cart matches like navigation is handled in
  // DidStartNavigation(). Some sites use GET requests though, so special-case
  // them here.
  GURL request_url = request.Url();
  bool should_use_dom_based_heuristics = ShouldUseDOMBasedHeuristics();
  bool add_to_cart_active = true;
  if (should_use_dom_based_heuristics) {
    add_to_cart_active = base::Time::Now() - add_to_cart_focus_time_ <
                         commerce::kAddToCartButtonActiveTime.Get();
  }
  if ((request.HttpMethod().Equals("POST") ||
       request_url.DomainIs(kEbayDomain) ||
       url.DomainIs(kElectronicExpressDomain)) &&
      add_to_cart_active) {
    bool is_add_to_cart = DetectAddToCart(render_frame(), request,
                                          should_use_dom_based_heuristics);
    OnWillSendRequest(render_frame(), is_add_to_cart);
  }

  // TODO(crbug.com/40163450): use MutationObserver on cart instead.
  // Detect XHR in cart page.
  // Don't do anything for subframes.
  if (frame->Parent())
    return;

  if (!url.SchemeIs(url::kHttpsScheme))
    return;
  if (IsVisitCart(url) && IsSameDomainXHR(url.host(), request)) {
    DVLOG(1) << "In-cart XHR: " << request.Url();
    MaybeExtractProducts();
  }
}

void CommerceHintAgent::DidStartNavigation(
    const GURL& url,
    std::optional<blink::WebNavigationType> navigation_type) {
  if (!url.SchemeIsHTTPOrHTTPS())
    return;
  should_use_dom_heuristics_.reset();
  has_finished_loading_ = false;
  starting_url_ = url;
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame());
  auto* observer_ptr = observer.get();
  observer_ptr->OnNavigation(
      url, CommerceHeuristicsData::GetInstance().GetVersion(),
      base::BindOnce(&CommerceHintAgent::DidStartNavigationCallback,
                     weak_factory_.GetWeakPtr(), url, std::move(observer)));
}

void CommerceHintAgent::DidStartNavigationCallback(
    const GURL& url,
    mojo::Remote<mojom::CommerceHintObserver> observer,
    bool should_skip,
    mojom::HeuristicsPtr heuristics) {
  should_skip_ = should_skip;
  if (should_skip)
    return;
  if (!heuristics->version_number.empty() &&
      heuristics->version_number !=
          CommerceHeuristicsData::GetInstance().GetVersion()) {
    bool is_populated =
        CommerceHeuristicsData::GetInstance().PopulateDataFromComponent(
            heuristics->hint_json_data, heuristics->global_json_data,
            /*product_id_json_data*/ "", /*cart_extraction_script*/ "");
    DCHECK(is_populated);
    CommerceHeuristicsData::GetInstance().UpdateVersion(
        base::Version(heuristics->version_number));
  }
}

void CommerceHintAgent::DidCommitProvisionalLoad(
    ui::PageTransition transition) {
  if (!starting_url_.is_valid())
    return;
  DCHECK(starting_url_.SchemeIsHTTPOrHTTPS());
  should_use_dom_heuristics_.reset();
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame());
  auto* observer_ptr = observer.get();
  observer_ptr->OnNavigation(
      starting_url_, CommerceHeuristicsData::GetInstance().GetVersion(),
      base::BindOnce(&CommerceHintAgent::DidCommitProvisionalLoadCallback,
                     weak_factory_.GetWeakPtr(), starting_url_,
                     std::move(observer)));
}

void CommerceHintAgent::DidCommitProvisionalLoadCallback(
    const GURL& url,
    mojo::Remote<mojom::CommerceHintObserver> observer,
    bool should_skip,
    mojom::HeuristicsPtr heuristics) {
  should_skip_ = should_skip;
  if (should_skip)
    return;
  if (!heuristics->version_number.empty() &&
      heuristics->version_number !=
          CommerceHeuristicsData::GetInstance().GetVersion()) {
    bool is_populated =
        CommerceHeuristicsData::GetInstance().PopulateDataFromComponent(
            heuristics->hint_json_data, heuristics->global_json_data,
            /*product_id_json_data*/ "", /*cart_extraction_script*/ "");
    DCHECK(is_populated);
    CommerceHeuristicsData::GetInstance().UpdateVersion(
        base::Version(heuristics->version_number));
  }
  // TODO(crbug.com/40061704): Add a test for when starting_url_ is invalid
  // because of multiple continuous DidCommitProvisionalLoad calls.
  if (!starting_url_.is_valid())
    return;
  if (IsAddToCart(starting_url_.PathForRequestPiece())) {
    RecordCommerceEvent(CommerceEvent::kAddToCartByURL);
    OnAddToCart(render_frame());
  }
  if (!IsVisitCart(starting_url_) && IsVisitCheckout(starting_url_)) {
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
  should_use_dom_heuristics_.reset();
  const GURL& url(frame->GetDocument().Url());
  if (!url.SchemeIs(url::kHttpsScheme))
    return;
  has_finished_loading_ = true;
  extraction_count_ = 0;
  mojo::Remote<mojom::CommerceHintObserver> observer =
      GetObserver(render_frame());
  auto* observer_ptr = observer.get();
  observer_ptr->OnNavigation(
      url, CommerceHeuristicsData::GetInstance().GetVersion(),
      base::BindOnce(&CommerceHintAgent::DidFinishLoadCallback,
                     weak_factory_.GetWeakPtr(), url, std::move(observer)));
}

void CommerceHintAgent::DidFinishLoadCallback(
    const GURL& url,
    mojo::Remote<mojom::CommerceHintObserver> observer,
    bool should_skip,
    mojom::HeuristicsPtr heuristics) {
  should_skip_ = should_skip;
  if (should_skip)
    return;
  if (!heuristics->version_number.empty() &&
      heuristics->version_number !=
          CommerceHeuristicsData::GetInstance().GetVersion()) {
    bool is_populated =
        CommerceHeuristicsData::GetInstance().PopulateDataFromComponent(
            heuristics->hint_json_data, heuristics->global_json_data,
            /*product_id_json_data*/ "", /*cart_extraction_script*/ "");
    DCHECK(is_populated);
    CommerceHeuristicsData::GetInstance().UpdateVersion(
        base::Version(heuristics->version_number));
  }
  // Some URLs might satisfy the patterns for both cart and checkout (e.g.
  // https://www.foo.com/cart/checkout). In those cases, cart has higher
  // priority.
  if (IsVisitCart(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCart);
    OnVisitCart(render_frame());
    DVLOG(1) << "Extract products after loading";
    MaybeExtractProducts();
  } else if (IsVisitCheckout(url)) {
    RecordCommerceEvent(CommerceEvent::kVisitCheckout);
    OnVisitCheckout(render_frame());
  }
}

void CommerceHintAgent::WillSubmitForm(const blink::WebFormElement& form) {
  if (!should_skip_.has_value() || should_skip_.value())
    return;
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIsHTTPOrHTTPS())
    return;

  bool is_purchase = false;
  for (const std::string& button_text : ExtractButtonTexts(form)) {
    if (IsPurchase(url, button_text)) {
      RecordCommerceEvent(CommerceEvent::kPurchaseByForm);
      OnPurchase(render_frame());
      is_purchase = true;
      break;
    }
  }
  OnFormSubmit(render_frame(), is_purchase);
}

// TODO(crbug.com/40163450): use MutationObserver on cart instead.
void CommerceHintAgent::ExtractCartFromCurrentFrame() {
  if (!should_skip_.has_value() || should_skip_.value())
    return;
  if (!has_finished_loading_)
    return;
  blink::WebLocalFrame* frame = render_frame()->GetWebFrame();
  // Don't do anything for subframes.
  if (frame->Parent())
    return;
  const GURL url(frame->GetDocument().Url());
  if (!url.SchemeIs(url::kHttpsScheme))
    return;

  if (IsVisitCart(url)) {
    DVLOG(1) << "Extract products due to layout shift or intersection change";
    MaybeExtractProducts();
  }
}

void CommerceHintAgent::DidObserveLayoutShift(double score,
                                              bool after_input_or_scroll) {
  DVLOG(1) << "Layout shift " << score << " " << after_input_or_scroll;
  ExtractCartFromCurrentFrame();
}

void CommerceHintAgent::OnMainFrameIntersectionChanged(
    const gfx::Rect& intersect_rect) {
  DVLOG(1) << "Intersection changed " << intersect_rect.x() << " "
           << intersect_rect.y() << " " << intersect_rect.width() << " "
           << intersect_rect.height();
  ExtractCartFromCurrentFrame();
}

void CommerceHintAgent::FocusedElementChanged(
    const blink::WebElement& focused_element) {
  // Don't observe focused element change when the navigation hasn't finished
  // to avoid being triggered by auto focus due to page rendering.
  if (!starting_url_.is_empty()) {
    return;
  }
  base::Time before_check = base::Time::Now();
  if ((before_check - add_to_cart_heuristics_execution_time_) <
      commerce::kHeuristicsExecutionGapTime.Get()) {
    return;
  }
  if (!should_skip_.has_value() || should_skip_.value()) {
    return;
  }
  if (!ShouldUseDOMBasedHeuristics()) {
    return;
  }
  auto builder = ukm::builders::Shopping_AddToCartDetection(
      render_frame()->GetWebFrame()->GetDocument().GetUkmSourceId());
  blink::WebElement element = focused_element;
  // Record the last time that the heuristics is run.
  add_to_cart_heuristics_execution_time_ = base::Time::Now();
  if (IsAddToCartButton(element)) {
    add_to_cart_focus_time_ = base::Time::Now();
  }
  base::TimeDelta execution_time = base::Time::Now() - before_check;
  base::UmaHistogramMicrosecondsTimes("Commerce.Carts.AddToCartButtonDetection",
                                      execution_time);
  builder.SetHeuristicsExecutionTime(execution_time.InMicroseconds())
      .Record(ukm_recorder_.get());
}

bool CommerceHintAgent::ShouldSkipAddToCartRequest(const GURL& navigation_url,
                                                   const GURL& request_url) {
  const std::string& navigation_domain = eTLDPlusOne(navigation_url);
  const re2::RE2* pattern =
      commerce_heuristics::CommerceHeuristicsData::GetInstance()
          .GetSkipAddToCartPatternForDomain(navigation_domain);
  if (pattern) {
    return RE2::PartialMatch(request_url.spec().substr(0, kLengthLimit),
                             *pattern);
  }
  const std::map<std::string, std::string>& skip_string_map =
      GetSkipAddToCartMapping();
  static base::NoDestructor<std::map<std::string, std::unique_ptr<re2::RE2>>>
      skip_regex_map;
  if (skip_string_map.find(navigation_domain) == skip_string_map.end()) {
    return false;
  }
  static re2::RE2::Options options;
  options.set_case_sensitive(false);
  if (skip_regex_map->find(navigation_domain) == skip_regex_map->end()) {
    skip_regex_map->insert(
        {navigation_domain,
         std::make_unique<re2::RE2>(skip_string_map.at(navigation_domain),
                                    options)});
  }
  return RE2::PartialMatch(request_url.spec().substr(0, kLengthLimit),
                           *skip_regex_map->at(navigation_domain));
}
}  // namespace cart
