// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_chrome_management_url_checker_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/version_info/channel.h"
#include "build/build_config.h"
#include "components/safe_search_api/url_checker_client.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
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
  if (!status.IsOk()) {
    DVLOG(1) << "ClassifyUrl request failed with status: " << status.ToString();
    std::move(client_callback)
        .Run(url, safe_search_api::ClientClassification::kUnknown);
    return;
  }

  DVLOG(1) << "URL classification = "
           << classify_url_response->display_classification();

  std::move(client_callback)
      .Run(url, ToSafeSearchClientClassification(classify_url_response.get()));
}

FetcherConfig GetFetcherConfig(bool is_subject_to_parental_controls) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // Supervised users on these platforms might get into a state where their
  // credentials are not available, so best-effort access mode is a graceful
  // fallback here.
  CHECK(is_subject_to_parental_controls)
      << "Kids API not available to users outside of Family Link parental "
         "controls";
  return kClassifyUrlConfigBestEffort;
#elif BUILDFLAG(IS_ANDROID)
  // Android enforces at the OS level that family link supervised users must
  // have valid sign in credentials (and triggers a reauth if not). We can
  // therefore wait for a valid access token to be available before calling
  // ClassifyUrl, to avoid window conditions where the access token is not yet
  // available (eg. during startup).
  if (is_subject_to_parental_controls ||
      !ClassifyUrlWithoutCredentialsForLocalSupervision()) {
    return kClassifyUrlConfigWaitUntilAccessTokenAvailable;
  }

  CHECK(!is_subject_to_parental_controls &&
        ClassifyUrlWithoutCredentialsForLocalSupervision())
      << "Mode not intended for family link users (or misconfigured "
         "experiment; enable kAllowNonFamilyLinkUrlFilterMode (backs "
         "ClassifyUrlWithoutCredentialsForLocalSupervision()))";
  return kClassifyUrlConfigWithoutCredentials;
#else
  // Other platforms use default configuration, which strictly requires
  // immediately available access token.
  CHECK(is_subject_to_parental_controls)
      << "Kids API not available to users outside of Family Link parental "
         "controls";
  return kClassifyUrlConfig;
#endif
}

// Flips order of arguments so that the unbound arguments will be the
// request and callback.
std::unique_ptr<ClassifyUrlFetcher> ClassifyURL(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const PrefService& pref_service_,
    version_info::Channel channel,
    const kidsmanagement::ClassifyUrlRequest& request,
    ClassifyUrlFetcher::Callback callback) {
  return CreateClassifyURLFetcher(
      *identity_manager, url_loader_factory, request, std::move(callback),
      GetFetcherConfig(IsSubjectToParentalControls(pref_service_)), channel);
}
}  // namespace

KidsChromeManagementURLCheckerClient::KidsChromeManagementURLCheckerClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const PrefService& pref_service_,
    std::string_view country,
    version_info::Channel channel)
    : country_(country),
      fetch_manager_(base::BindRepeating(&ClassifyURL,
                                         identity_manager,
                                         url_loader_factory,
                                         std::ref(pref_service_),
                                         channel)) {}

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
}
}  // namespace supervised_user
