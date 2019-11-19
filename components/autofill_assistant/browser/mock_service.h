// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_

#include <map>
#include <string>
#include <vector>

#include "components/autofill_assistant/browser/service_impl.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

// TODO(crbug.com/806868): inherit from |Service| instead, and properly mock
// methods as necessary.
class MockService : public ServiceImpl {
 public:
  MockService();
  ~MockService() override;

  void GetScriptsForUrl(const GURL& url,
                        const TriggerContext& trigger_context,
                        ResponseCallback callback) override {
    // Transforming callback into a references allows using RunOnceCallback on
    // the argument.
    OnGetScriptsForUrl(url, trigger_context, callback);
  }
  MOCK_METHOD3(OnGetScriptsForUrl,
               void(const GURL& url,
                    const TriggerContext& trigger_context,
                    ResponseCallback& callback));

  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ResponseCallback callback) override {
    OnGetActions(script_path, url, trigger_context, global_payload,
                 script_payload, callback);
  }
  MOCK_METHOD6(OnGetActions,
               void(const std::string& script_path,
                    const GURL& url,
                    const TriggerContext& trigger_contexts,
                    const std::string& global_payload,
                    const std::string& script_payload,
                    ResponseCallback& callback));

  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override {
    OnGetNextActions(trigger_context, previous_global_payload,
                     previous_script_payload, processed_actions, callback);
  }
  MOCK_METHOD5(OnGetNextActions,
               void(const TriggerContext& trigger_contexts,
                    const std::string& previous_global_payload,
                    const std::string& previous_script_payload,
                    const std::vector<ProcessedActionProto>& processed_actions,
                    ResponseCallback& callback));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_SERVICE_H_
