// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_TEST_ENDPOINT_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_TEST_ENDPOINT_SERVICE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/service/service.h"
#include "components/autofill_assistant/browser/user_data.h"

namespace autofill_assistant {

// TODO(arbesser) Move this to chrome/browser/android, it does not belong into
// components/autofill_assistant.
//
// Thin C++ wrapper around a regular service that talks to a real, but non-prod
// endpoint. Intended for manual testing only, to run integration tests against
// real websites. Automatically disables animations as well.
class JavaTestEndpointService : public Service {
 public:
  explicit JavaTestEndpointService(std::unique_ptr<Service> service_impl);
  ~JavaTestEndpointService() override;
  JavaTestEndpointService(const JavaTestEndpointService&) = delete;
  JavaTestEndpointService& operator=(const JavaTestEndpointService&) = delete;

  void GetScriptsForUrl(const GURL& url,
                        const TriggerContext& trigger_context,
                        ResponseCallback callback) override;

  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ResponseCallback callback) override;

  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      const RoundtripTimingStats& timing_stats,
      ResponseCallback callback) override;

  void GetUserData(const CollectUserDataOptions& options,
                   uint64_t run_id,
                   ResponseCallback callback) override;

 private:
  void OnGetScriptsForUrl(ResponseCallback callback,
                          int http_status,
                          const std::string& response);

  std::unique_ptr<Service> service_impl_;
  base::WeakPtrFactory<JavaTestEndpointService> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SERVICE_JAVA_TEST_ENDPOINT_SERVICE_H_
