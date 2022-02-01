// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_ONBOARDING_FETCHER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_ONBOARDING_FETCHER_H_

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace autofill_assistant {

extern const char kDefaultOnboardingDataUrlPattern[];

// Used to fetch the configuration for the onboarding screen to be shown on the
// user's first run of Autofill Assistant.
class AutofillAssistantOnboardingFetcher : public KeyedService {
 public:
  using ResponseCallback =
      base::OnceCallback<void(const base::flat_map<std::string, std::string>&)>;

  using StringMap =
      base::flat_map<std::string, base::flat_map<std::string, std::string>>;

  AutofillAssistantOnboardingFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  ~AutofillAssistantOnboardingFetcher() override;

  void FetchOnboardingDefinition(const std::string& intent,
                                 const std::string& locale,
                                 int timeout_ms,
                                 ResponseCallback callback);

 private:
  // Sends new request to gstatic.
  void StartFetch(const std::string& locale, int timeout_ms);

  // Callback for the request to gstatic.
  void OnFetchComplete(std::unique_ptr<std::string> response_body);

  // Parses the response body. Returns a result status.
  Metrics::OnboardingFetcherResultStatus ParseResponse(
      std::unique_ptr<std::string> response_body);

  // Extracts the requested data and runs the callback.
  void RunCallback(const std::string& intent, ResponseCallback callback);

  std::vector<base::OnceClosure> pending_callbacks_;

  // Contains the onboarding data as a map of:
  // {
  //   "intent": {
  //     "string-id": "string-value"
  //   }
  // }
  StringMap onboarding_strings_;

  // URL loader object for the gstatic request. If |url_loader_| is not
  // null, a request is currently in flight.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // Used for the gstatic requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_AUTOFILL_ASSISTANT_ONBOARDING_FETCHER_H_
