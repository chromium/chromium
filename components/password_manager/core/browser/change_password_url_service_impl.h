// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/timer/elapsed_timer.h"
#include "components/password_manager/core/browser/change_password_url_service.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;
class PrefService;

namespace network {
class SharedURLLoaderFactory;
}

namespace password_manager {

extern const char kGetChangePasswordUrlMetricName[];
extern const char kChangePasswordUrlServiceFetchResultMetricName[];
extern const char kGstaticFetchErrorCodeMetricName[];
extern const char kGstaticFetchHttpResponseCodeMetricName[];
extern const char kGstaticFetchTimeMetricName[];

// Used to log the response of the request to the gstatic file. These values are
// persisted to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class ChangePasswordUrlServiceFetchResult {
  kSuccess = 0,
  kFailure = 1,
  kMalformed = 2,
  kMaxValue = kMalformed,
};

class ChangePasswordUrlServiceImpl
    : public password_manager::ChangePasswordUrlService {
 public:
  explicit ChangePasswordUrlServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service);
  ~ChangePasswordUrlServiceImpl() override;

  // ChangePasswordUrlService:
  void PrefetchURLs() override;
  GURL GetChangePasswordUrl(const GURL& url) override;

  static constexpr char kChangePasswordUrlOverrideUrl[] =
      "https://www.gstatic.com/chrome/password-manager/"
      "change_password_urls.json";

 private:
  enum class FetchState {
    // Default state, no request started.
    kNoRequestStarted,
    // Active while gstatic file is fetched.
    kIsLoading,
    // Set when the fetch succeeded.
    kFetchSucceeded,
    // Set when the fetch failed.
    kFetchFailed,
    // Set when the password manager is disabled and the gstatic file is not
    // fetched.
    kUrlOverridesDisabled,
  };
  // Callback for the the request to gstatic.
  void OnFetchComplete(std::unique_ptr<std::string> response_body);

  FetchState state_ = FetchState::kNoRequestStarted;
  // Stores the JSON result for the url overrides.
  base::flat_map<std::string, GURL> change_password_url_map_;
  // URL loader object for the gstatic request.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  // SharedURLLoaderFactory for the gstatic request, argument in the
  // constructor.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  PrefService* pref_service_;
  // Timer to track the response time of the gstatic request.
  base::ElapsedTimer fetch_timer_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CHANGE_PASSWORD_URL_SERVICE_IMPL_H_
