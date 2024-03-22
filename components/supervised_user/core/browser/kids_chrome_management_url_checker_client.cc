// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/common/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/protobuf/src/google/protobuf/message_lite.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

using kidsmanagement::ClassifyUrlResponse;

safe_search_api::ClientClassification ToSafeSearchClientClassification(
    ClassifyUrlResponse* classify_url_response) {
  switch (classify_url_response->display_classification()) {
    case ClassifyUrlResponse::UNKNOWN_DISPLAY_CLASSIFICATION:
      return safe_search_api::ClientClassification::kUnknown;
    case ClassifyUrlResponse::RESTRICTED:
      return safe_search_api::ClientClassification::kRestricted;
    case ClassifyUrlResponse::ALLOWED:
      return safe_search_api::ClientClassification::kAllowed;
  }
}

void OnResponse(
    const GURL& url,
    safe_search_api::URLCheckerClient::ClientCheckCallback client_callback,
    const ProtoFetcherStatus& status,
    std::unique_ptr<kidsmanagement::ClassifyUrlResponse>
        classify_url_response) {
  DVLOG(1) << "URL classification = "
           << classify_url_response->display_classification();

  if (!status.IsOk()) {
    DVLOG(1) << "ClassifyUrl request failed with status: " << status.ToString();
    std::move(client_callback)
        .Run(url, safe_search_api::ClientClassification::kUnknown);
    return;
  }

  std::move(client_callback)
      .Run(url, ToSafeSearchClientClassification(classify_url_response.get()));
}

// Flips order of arguments so that the sole unbound argument will be the
// request.
std::unique_ptr<ProtoFetcher<kidsmanagement::ClassifyUrlResponse>> ClassifyURL(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const FetcherConfig& config,
    const kidsmanagement::ClassifyUrlRequest& request) {
  return CreateClassifyURLFetcher(*identity_manager, url_loader_factory,
                                  request, config);
}

FetcherConfig GetFetcherConfig() {
  if (base::FeatureList::IsEnabled(
          kWaitUntilAccessTokenAvailableForClassifyUrl)) {
    return kClassifyUrlConfigWaitUntilAccessTokenAvailable;
  }
  return kClassifyUrlConfig;
}

}  // namespace

KidsChromeManagementURLCheckerClient::KidsChromeManagementURLCheckerClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::string_view country)
    : safe_search_client_(url_loader_factory,
                          kClassifyUrlConfig.traffic_annotation()),
      country_(country),
      fetch_manager_(base::BindRepeating(&ClassifyURL,
                                         identity_manager,
                                         url_loader_factory,
                                         GetFetcherConfig())) {}

KidsChromeManagementURLCheckerClient::~KidsChromeManagementURLCheckerClient() =
    default;

void KidsChromeManagementURLCheckerClient::CheckURL(
    const GURL& url,
    safe_search_api::URLCheckerClient::ClientCheckCallback callback) {
  kidsmanagement::ClassifyUrlRequest request;
  request.set_url(url.spec());
  request.set_region_code(country_);

  fetch_manager_.Fetch(request,
                       base::BindOnce(&OnResponse, url, std::move(callback)));

  if (IsShadowKidsApiWithSafeSitesEnabled()) {
    // Actual client is timing the latency in Enterprise.SafeSites.Latency
    safe_search_client_.CheckURL(url, base::DoNothing());
  }
}
}  // namespace supervised_user
