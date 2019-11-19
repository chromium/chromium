// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_fetcher.h"

#include "base/bind.h"
#include "base/macros.h"
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
                  const base::string16& keyword,
                  const GURL& osdd_url,
                  const GURL& favicon_url,
                  const url::Origin& initiator,
                  network::mojom::URLLoaderFactory* url_loader_factory,
                  int render_frame_id,
                  int resource_type);

  // If data contains a valid OSDD, a TemplateURL is created and added to
  // the TemplateURLService.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // URL of the OSDD.
  GURL url() const { return osdd_url_; }

  // Keyword to use.
  base::string16 keyword() const { return keyword_; }

 private:
  void OnTemplateURLParsed(std::unique_ptr<TemplateURL> template_url);
  void OnLoaded();
  void AddSearchProvider();

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  TemplateURLFetcher* fetcher_;
  std::unique_ptr<TemplateURL> template_url_;
  base::string16 keyword_;
  const GURL osdd_url_;
  const GURL favicon_url_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_subscription_;

  DISALLOW_COPY_AND_ASSIGN(RequestDelegate);
};

TemplateURLFetcher::RequestDelegate::RequestDelegate(
    TemplateURLFetcher* fetcher,
    const base::string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const url::Origin& initiator,
    network::mojom::URLLoaderFactory* url_loader_factory,
    int render_frame_id,
    int resource_type)
    : fetcher_(fetcher),
      keyword_(keyword),
      osdd_url_(osdd_url),
      favicon_url_(favicon_url) {
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);  // TemplateURLFetcher::ScheduleDownload verifies this.

  if (!model->loaded()) {
    // Start the model load and set-up waiting for it.
    template_url_subscription_ = model->RegisterOnLoadedCallback(
        base::Bind(&TemplateURLFetcher::RequestDelegate::OnLoaded,
                   base::Unretained(this)));
    model->Load();
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = osdd_url;
  resource_request->request_initiator = initiator;
  resource_request->render_frame_id = render_frame_id;
  resource_request->resource_type = resource_type;
  resource_request->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kTrafficAnnotation);
  simple_url_loader_->SetAllowHttpErrorResults(true);
  simple_url_loader_->SetTimeoutDuration(
      base::TimeDelta::FromSeconds(kOpenSearchTimeoutSeconds));
  simple_url_loader_->SetRetryOptions(
      kOpenSearchRetryCount, network::SimpleURLLoader::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->DownloadToString(
      url_loader_factory,
      base::BindOnce(
          &TemplateURLFetcher::RequestDelegate::OnSimpleLoaderComplete,
          base::Unretained(this)),
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
  template_url_subscription_.reset();
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
                     base::Unretained(this)));
}

void TemplateURLFetcher::RequestDelegate::AddSearchProvider() {
  DCHECK(template_url_);
  DCHECK(!keyword_.empty());
  TemplateURLService* model = fetcher_->template_url_service_;
  DCHECK(model);
  DCHECK(model->loaded());

  const TemplateURL* existing_url = nullptr;
  if (!model->CanAddAutogeneratedKeyword(keyword_, GURL(template_url_->url()),
                                         &existing_url)) {
    fetcher_->RequestCompleted(this);  // WARNING: Deletes us!
    return;
  }

  if (existing_url)
    model->Remove(existing_url);

  // The short name is what is shown to the user. We preserve original names
  // since it is better when generated keyword in many cases.
  TemplateURLData data(template_url_->data());
  data.SetKeyword(keyword_);
  data.originating_url = osdd_url_;

  // The page may have specified a URL to use for favicons, if not, set it.
  if (!data.favicon_url.is_valid())
    data.favicon_url = favicon_url_;

  // Mark the keyword as replaceable so it can be removed if necessary.
  data.safe_for_autoreplace = true;
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
    const base::string16& keyword,
    const GURL& osdd_url,
    const GURL& favicon_url,
    const url::Origin& initiator,
    network::mojom::URLLoaderFactory* url_loader_factory,
    int render_frame_id,
    int resource_type) {
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
      render_frame_id, resource_type));
}

void TemplateURLFetcher::RequestCompleted(RequestDelegate* request) {
  auto i = std::find_if(requests_.begin(), requests_.end(),
                        [request](const std::unique_ptr<RequestDelegate>& ptr) {
                          return ptr.get() == request;
                        });
  DCHECK(i != requests_.end());
  requests_.erase(i);
}
