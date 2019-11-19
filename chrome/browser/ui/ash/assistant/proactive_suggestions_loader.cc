// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/proactive_suggestions_loader.h"

#include "ash/public/cpp/assistant/proactive_suggestions.h"
#include "ash/public/cpp/assistant/util/histogram_util.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/services/assistant/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr net::NetworkTrafficAnnotationTag kNetworkTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("proactive_suggestions_loader", R"(
          semantics: {
            sender: "Google Assistant Proactive Suggestions"
            description:
              "The Google Assistant requests proactive content suggestions "
              "based on the currently active browser context."
            trigger:
              "Change of active browser context."
            destination: GOOGLE_OWNED_SERVICE
          }
          policy: {
            cookies_allowed: YES
            setting:
              "The Google Assistant can be enabled/disabled in Chrome Settings "
              "and is subject to eligibility requirements. The user may also "
              "separately opt out of sharing screen context with Assistant."
          })");

// Helpers ---------------------------------------------------------------------

// Returns the url to retrieve proactive content suggestions for a given |url|.
GURL CreateProactiveSuggestionsUrl(const GURL& url) {
  constexpr char kProactiveSuggestionsEndpoint[] =
      "https://assistant.google.com/proactivesuggestions/embeddedview";
  GURL result = GURL(kProactiveSuggestionsEndpoint);

  // The proactive suggestions service needs to be told which experiments, if
  // any, to trigger on the backend.
  const std::string experiment_ids = chromeos::assistant::features::
      GetProactiveSuggestionsServerExperimentIds();
  if (!experiment_ids.empty()) {
    constexpr char kExperimentIdsParamKey[] = "experimentIds";
    result = net::AppendOrReplaceQueryParameter(result, kExperimentIdsParamKey,
                                                experiment_ids);
  }

  // The proactive suggestions service needs to be aware of the device locale.
  constexpr char kLocaleParamKey[] = "hl";
  result = net::AppendOrReplaceQueryParameter(
      result, kLocaleParamKey, base::i18n::GetConfiguredLocale());

  // The proactive suggestions service needs to be informed of the given |url|.
  constexpr char kUrlParamKey[] = "url";
  return net::AppendOrReplaceQueryParameter(result, kUrlParamKey, url.spec());
}

// Parses proactive suggestions metadata from the specified |headers|.
void ParseProactiveSuggestionsMetadata(const net::HttpResponseHeaders& headers,
                                       int* category,
                                       std::string* description,
                                       std::string* search_query,
                                       bool* has_content) {
  constexpr char kCategoryHeaderName[] =
      "x-assistant-proactive-suggestions-page-category";
  constexpr char kDescriptionHeaderName[] =
      "x-assistant-proactive-suggestions-description";
  constexpr char kHasContentHeaderName[] =
      "x-assistant-proactive-suggestions-has-ui-content";
  constexpr char kSearchQueryHeaderName[] =
      "x-assistant-proactive-suggestions-search-query";

  DCHECK_EQ(ash::ProactiveSuggestions::kCategoryUnknown, *category);
  DCHECK(description->empty());
  DCHECK(search_query->empty());
  DCHECK(!(*has_content));

  size_t it = 0;
  std::string name;
  std::string value;

  while (headers.EnumerateHeaderLines(&it, &name, &value)) {
    if (name == kCategoryHeaderName) {
      DCHECK_EQ(ash::ProactiveSuggestions::kCategoryUnknown, *category);
      if (!base::StringToInt(value, category))
        NOTREACHED();
      continue;
    }
    if (name == kDescriptionHeaderName) {
      DCHECK(description->empty());
      *description = value;
      continue;
    }
    if (name == kHasContentHeaderName) {
      DCHECK(!(*has_content));
      *has_content = (value == "1");
      continue;
    }
    if (name == kSearchQueryHeaderName) {
      DCHECK(search_query->empty());
      *search_query = value;
    }
  }
}

}  // namespace

// ProactiveSuggestionsLoader --------------------------------------------------

ProactiveSuggestionsLoader::ProactiveSuggestionsLoader(Profile* profile,
                                                       const GURL& url)
    : profile_(profile), url_(url) {}

ProactiveSuggestionsLoader::~ProactiveSuggestionsLoader() {
  if (complete_callback_)
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
}

void ProactiveSuggestionsLoader::Start(CompleteCallback complete_callback) {
  DCHECK(!loader_);
  complete_callback_ = std::move(complete_callback);

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = CreateProactiveSuggestionsUrl(url_);

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             kNetworkTrafficAnnotationTag);

  auto* url_loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(profile_)
          ->GetURLLoaderFactoryForBrowserProcess()
          .get();

  auto simple_url_loader_complete_callback =
      base::BindOnce(&ProactiveSuggestionsLoader::OnSimpleURLLoaderComplete,
                     base::Unretained(this));

  loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory, std::move(simple_url_loader_complete_callback));
}

void ProactiveSuggestionsLoader::OnSimpleURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  using ash::assistant::metrics::ProactiveSuggestionsRequestResult;

  // When the request fails to return a valid response we record the event and
  // return a null set of proactive suggestions.
  if (!response_body || loader_->NetError() != net::OK ||
      !loader_->ResponseInfo() || !loader_->ResponseInfo()->headers) {
    ash::assistant::metrics::RecordProactiveSuggestionsRequestResult(
        ash::ProactiveSuggestions::kCategoryUnknown,
        ProactiveSuggestionsRequestResult::kError);
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
    return;
  }

  // The proactive suggestions server will return metadata in the HTTP headers.
  int category = ash::ProactiveSuggestions::kCategoryUnknown;
  std::string description;
  std::string search_query;
  bool has_content = false;
  ParseProactiveSuggestionsMetadata(*loader_->ResponseInfo()->headers,
                                    &category, &description, &search_query,
                                    &has_content);

  // When the server indicates that there is no content we record the event and
  // return a null set of proactive suggestions.
  if (!has_content) {
    ash::assistant::metrics::RecordProactiveSuggestionsRequestResult(
        category, ProactiveSuggestionsRequestResult::kSuccessWithoutContent);
    std::move(complete_callback_).Run(/*proactive_suggestions=*/nullptr);
    return;
  }

  // Otherwise we have a populated proactive suggestions response so we record
  // the event and return a fully constructed set of proactive suggestions.
  ash::assistant::metrics::RecordProactiveSuggestionsRequestResult(
      category, ProactiveSuggestionsRequestResult::kSuccessWithContent);
  std::move(complete_callback_)
      .Run(base::MakeRefCounted<ash::ProactiveSuggestions>(
          category, std::move(description), std::move(search_query),
          std::move(*response_body)));
}
