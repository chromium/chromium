// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_piece.h"
#include "base/types/strong_alias.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/supervised_user/core/browser/kids_external_fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// Overview: KidsExternalFetcher provides an interface for generic fetchers that
// use classes to represent Request and Response objects. The default mechanism
// under the hood takes care of the fetch process, including:
// * obtaining the right access token,
// * serializing the request and parsing the response,
// * submitting metrics.
//
// If you want to create new fetcher factory method, then some
// details must be provided in order to enable fetching for said <Request,
// Response> pair. The new fetcher factory should have at least the following
// arguments: signin::IdentityManager, network::SharedURLLoaderFactory,
// consuming callback and must reference a static configuration.
//
// The static configuration should be placed in the
// kids_external_fetcher_config.h module.

// Holds the status of the fetch. The callback's response will be set iff the
// status is ok.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
class KidsExternalFetcherStatus {
 public:
  using HttpStatusOrNetErrorType =
      base::StrongAlias<class HttpStatusOrNetErrorTag, int>;

  enum State {
    OK = 0,
    GOOGLE_SERVICE_AUTH_ERROR = 1,  // Error occurred during the access token
                                    // fetching phase. See
                                    // GetGoogleServiceAuthError for details.
    HTTP_STATUS_OR_NET_ERROR =
        2,  // The request was performed, but network or http returned errors.
            // This is default chromium approach to combine those two domains.
    INVALID_RESPONSE = 3,  // The request was performed without error, but http
                           // response could not be processed or was unexpected.
    DATA_ERROR = 4,  // The request was parsed, but did not contain all required
                     // data. Not signalled by this fetcher itself, but might be
                     // used by consumers to indicate data problem.
    kMaxValue = DATA_ERROR,  // keep last, required for metrics.
  };

  // Status might be used in base::expected context as possible error, since it
  // contains two error-enabled attributes which are copyable / assignable.
  KidsExternalFetcherStatus(const KidsExternalFetcherStatus&);
  KidsExternalFetcherStatus& operator=(const KidsExternalFetcherStatus&);

  ~KidsExternalFetcherStatus();
  KidsExternalFetcherStatus() = delete;

  // Convenience creators instead of exposing KidsExternalFetcherStatus(State
  // state).
  static KidsExternalFetcherStatus Ok();
  static KidsExternalFetcherStatus GoogleServiceAuthError(
      GoogleServiceAuthError
          error);  // The copy follows the interface of
                   // https://source.chromium.org/chromium/chromium/src/+/main:components/signin/public/identity_manager/primary_account_access_token_fetcher.h;l=241;drc=8ba1bad80dc22235693a0dd41fe55c0fd2dbdabd
  static KidsExternalFetcherStatus HttpStatusOrNetError(
      int value = 0);  // Either net::Error (negative numbers, 0 denotes
                       // success) or HTTP status.
  static KidsExternalFetcherStatus InvalidResponse();
  static KidsExternalFetcherStatus DataError();

  // KidsExternalFetcherStatus::IsOk iff google_service_auth_error_.state() ==
  // NONE and state_ == NONE
  bool IsOk() const;
  // Indicates whether the status is not ok, but is worth retrying because it
  // might go away.
  bool IsTransientError() const;
  // Indicates whether the status is not ok and there is no point in retrying.
  bool IsPersistentError() const;

  // Returns a message describing the status.
  std::string ToString() const;

  // Translate the status to metric enum label as defined in
  // tools/metrics/histograms/enums.xml.
  std::string ToMetricEnumLabel() const;

  State state() const;
  HttpStatusOrNetErrorType http_status_or_net_error() const;
  const class GoogleServiceAuthError& google_service_auth_error() const;

 private:
  // Disallows impossible states.
  explicit KidsExternalFetcherStatus(State state);
  explicit KidsExternalFetcherStatus(
      HttpStatusOrNetErrorType http_status_or_net_error);
  explicit KidsExternalFetcherStatus(
      class GoogleServiceAuthError
          google_service_auth_error);  // Implies State ==
                                       // GOOGLE_SERVICE_AUTH_ERROR
  KidsExternalFetcherStatus(
      State state,
      class GoogleServiceAuthError google_service_auth_error);

  State state_;
  HttpStatusOrNetErrorType http_status_or_net_error_{
      0};  // Meaningful iff state_ == HTTP_STATUS_OR_NET_ERROR
  class GoogleServiceAuthError google_service_auth_error_;
};

// Use instance of Fetcher to start request and write the result onto the
// receiving delegate. Every instance of Fetcher is disposable and should be
// used only once.
template <typename Request, typename Response>
class KidsExternalFetcher {
 public:
  using Callback = base::OnceCallback<void(KidsExternalFetcherStatus,
                                           std::unique_ptr<Response>)>;
  virtual ~KidsExternalFetcher() = default;
};

// Creates a disposable instance of an access token consumer that will fetch
// list of family members.
std::unique_ptr<
    KidsExternalFetcher<kids_chrome_management::ListFamilyMembersRequest,
                        kids_chrome_management::ListFamilyMembersResponse>>
FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    KidsExternalFetcher<
        kids_chrome_management::ListFamilyMembersRequest,
        kids_chrome_management::ListFamilyMembersResponse>::Callback callback,
    const supervised_user::FetcherConfig& config =
        supervised_user::kListFamilyMembersConfig);

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_EXTERNAL_FETCHER_H_
