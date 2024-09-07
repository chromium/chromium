// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_fetcher.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_parser.h"
#include "components/search_engines/template_url_service.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace {

// In some network environments, silent failure can be avoided by retrying
// request on network change. This helps OpenSearch get through in such cases.
// See https://crbug.com/956689 for context.
constexpr int kOpenSearchRetryCount = 3;

// Timeout for OpenSearch description document (OSDD) fetch request.
// Requests for a particular resource are limited to one.
// Requests may not receive a response, and in that case no
// further requests would be allowed. The timeout cleans up failed requests
// so that later attempts to fetch the OSDD can be made.
constexpr int kOpenSearchTimeoutSeconds = 30;

// Traffic annotation for RequestDelegate.
const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("open_search", R"(
      semantics {
        sender: "Omnibox"
        description:
          "Web pages can include an OpenSearch description doc in their HTML. "
          "In this case Chromium downloads and parses the file. The "
          "corresponding search engine is added to the list in the browser "
          "settings (chrome://settings/searchEngines)."
        trigger:
          "User visits a web page containing a <link rel=\"search\"> tag."
        data: "None"
        destination: WEBSITE
      }
      policy {
        cookies_allowed: YES
        cookies_store: "user"
        setting: "This feature cannot be disabled in settings."
        policy_exception_justification:
          "Not implemented, considered not useful as this feature does not "
          "upload any data."
      })");

}  // namespace

// RequestDelegate ------------------------------------------------------------
class TemplateURLFetcher::RequestDelegate {
 public:
  RequestDelegate(TemplateURLFetcher* fetcher,
                  const std::u16string& keyword,
                  const GURL& osdd_url,
                  const GURL& favicon_url,
                  const url::Origin& initiator,
                  network::mojom::URLLoaderFactory* url_loader_factory,
                  int render_frame_id,
                  int32_t request_id);

  RequestDelegate(const RequestDelegate&) = delete;
  RequestDelegate& operator=(const RequestDelegate&) = delete;

  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLService.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // URL of the OSDD.
  GURL url() const { return osdd_url_; }

  // Keyword to use.
  std::u16string keyword() const { return keyword_; }

 private:
  void OnTemplateURLParsed(std::unique_ptr<TemplateURL> template_url);
  void OnLoaded();
  void AddSearchProvider();

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  raw_ptr<TemplateURLFetcher> fetcher_;
  std::unique_ptr<TemplateURL> template_url_;
  std::u16string keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;

  base::CallbackListSubscription template_url_subscription_;

  base::WeakPtrFactory<RequestDelegate> weak_factory_{this};
};

TemplateURLFetcher::RequestDelegate::RequestDelegate(
    TemplateURLFetcher* fetcher,
    const std::u16string& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const url::Origin& initiator,
    network::mojom::URLLoaderFactory* url_loader_factory,
    int render_frame_id,
    int32_t request_id)
    : fetcher_(fetcher),
      keyword_(keyword),
      osdd_url_(osdd_url),
      favicon_url_(favicon_url) {
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);  // TemplateURLFetcher::ScheduleDownload verifies this.

  if (!model->loaded()) {
    // Start the model load and set-up waiting for it.
    template_url_subscription_ = model->RegisterOnLoadedCallback(
        base::BindOnce(&TemplateURLFetcher::RequestDelegate::OnLoaded,
                       weak_factory_.GetWeakPtr()));
    model->Load();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = osdd_url;
  resource_request->request_initiator = initiator;
  // TODO(crbug.com/40121693): Remove |resource_type| once the request is
  // handled with RequestDestination without ResourceType.
  resource_request->resource_type =
      /* blink::mojom::ResourceType::kSubResource */ 6;
  resource_request->destination = network::mojom::RequestDestination::kEmpty;
  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->SetTimeoutDuration(
      base::Seconds(kOpenSearchTimeoutSeconds));
  simple_url_loader_->SetRetryOptions(
      kOpenSearchRetryCount, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetRequestID(request_id);
  simple_url_loader_->DownloadToString(
      url_loader_factory,
      base::BindOnce(
          &TemplateURLFetcher::RequestDelegate::OnSimpleLoaderComplete,
          weak_factory_.GetWeakPtr()),
      50000 /* max_body_size */);
}

void TemplateURLFetcher::RequestDelegate::OnTemplateURLParsed(
    std::unique_ptr<TemplateURL> template_url) {
  template_url_ = std::move(template_url);

  if (!template_url_ ||
      !template_url_->url_ref().SupportsReplacement(
          fetcher_->template_url_service_->search_terms_data())) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  // Wait for the model to be loaded before adding the provider.
  if (!fetcher_->template_url_service_->loaded())
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::OnLoaded() {
  template_url_subscription_ = {};
  if (!template_url_)
    return;
  AddSearchProvider();
  // WARNING: AddSearchProvider deletes us.
}

void TemplateURLFetcher::RequestDelegate::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // Validation checks.
  // Make sure we can still replace the keyword, i.e. the fetch was successful.
  if (!response_body) {
    fetcher_->RequestCompleted(this);
    // WARNING: RequestCompleted deletes us.
    return;
  }

  TemplateURLParser::Parse(
      &fetcher_->template_url_service_->search_terms_data(),
      *response_body.get(), TemplateURLParser::ParameterFilter(),
      base::BindOnce(&RequestDelegate::OnTemplateURLParsed,
                     weak_factory_.GetWeakPtr()));
}

void TemplateURLFetcher::RequestDelegate::AddSearchProvider() {
  DCHECK(template_url_);
  DCHECK(!keyword_.empty());
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);
  DCHECK(model->loaded());

  if (!model->CanAddAutogeneratedKeyword(keyword_,
                                         GURL(template_url_->url()))) {
    fetcher_->RequestCompleted(this);  // WARNING: Deletes us!
    return;
  }

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  TemplateURLData data(template_url_->data());
  data.SetKeyword(keyword_);
  data.originating_url = osdd_url_;

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!data.favicon_url.is_valid())
    data.favicon_url = favicon_url_;

  // Mark the keyword as replaceable so it can be removed if necessary.
  // Add() will automatically remove conflicting keyword replaceable engines.
  data.safe_for_autoreplace = true;

  // Autogenerated keywords are kUnspecified active status by default.
  // kUnspecified keywords are inactive and cannot be triggered in the
  // omnibox until they are activated.
  data.is_active = TemplateURLData::ActiveStatus::kUnspecified;

  model->Add(std::make_unique<TemplateURL>(data));

  fetcher_->RequestCompleted(this);
  // WARNING: RequestCompleted deletes us.
}

// TemplateURLFetcher ---------------------------------------------------------

TemplateURLFetcher::TemplateURLFetcher(TemplateURLService* template_url_service)
    : template_url_service_(template_url_service) {}

TemplateURLFetcher::~TemplateURLFetcher() {
}

void TemplateURLFetcher::ScheduleDownload(
    const std::u16string& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const url::Origin& initiator,
    network::mojom::URLLoaderFactory* url_loader_factory,
    int render_frame_id,
    int32_t request_id) {
  DCHECK(osdd_url.is_valid());
  DCHECK(!keyword.empty());

  if (!template_url_service_->loaded()) {
    // We could try to set up a callback to this function again once the model
    // is loaded but meh.
    template_url_service_->Load();
    return;
  }

  const TemplateURL* template_url =
      template_url_service_->GetTemplateURLForKeyword(keyword);
  if (template_url && (!template_url->safe_for_autoreplace() ||
                       template_url->originating_url() == osdd_url))
    return;

  // Make sure we aren't already downloading this request.
  for (const auto& request : requests_) {
    if ((request->url() == osdd_url) || (request->keyword() == keyword))
      return;
  }

  requests_.push_back(std::make_unique<RequestDelegate>(
      this, keyword, osdd_url, favicon_url, initiator, url_loader_factory,
      render_frame_id, request_id));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  auto i = base::ranges::find(requests_, request,
                              &std::unique_ptr<RequestDelegate>::get);
  CHECK(i != requests_.end(), base::NotFatalUntil::M130);
  requests_.erase(i);
}
