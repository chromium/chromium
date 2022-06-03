// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "components/autofill_assistant/browser/public/external_action_delegate.h"
#include "components/autofill_assistant/browser/public/external_script_controller.h"

namespace content {
class WebContents;
}

namespace autofill_assistant {

// Abstract interface for exported services.
class AutofillAssistant {
 public:
  struct CapabilitiesInfo {
    CapabilitiesInfo();
    CapabilitiesInfo(
        const std::string& url,
        const base::flat_map<std::string, std::string>& script_parameters);
    ~CapabilitiesInfo();
    CapabilitiesInfo(const CapabilitiesInfo& other);
    CapabilitiesInfo& operator=(const CapabilitiesInfo& other);

    std::string url;
    base::flat_map<std::string, std::string> script_parameters;
  };

  using GetCapabilitiesResponseCallback =
      base::OnceCallback<void(int http_status,
                              const std::vector<CapabilitiesInfo>&)>;

  virtual ~AutofillAssistant() = default;

  // Allows querying for domain capabilities by sending the |hash_prefix_length|
  // number of leading bits of the domain url hashes. CityHash64 should be used
  // to calculate the hashes and only the leading |hash_prefix_length| bits
  // should be sent.
  // |intent| should contain the string representation of the enum:
  // https://source.corp.google.com/piper///depot/google3/quality/genie/autobot/dev/proto/script/intent.proto
  virtual void GetCapabilitiesByHashPrefix(
      uint32_t hash_prefix_length,
      const std::vector<uint64_t>& hash_prefix,
      const std::string& intent,
      GetCapabilitiesResponseCallback callback) = 0;

  // Returns an |ExternalScriptController| which can be used to execute scripts
  // on the tab specified by |web_contents|, by calling
  // |ExternalScriptController::StartScript|.
  // The returned |ExternalScriptController| instance has to survive for the
  // duration of the execution of the script.
  // |action_extension_delegate| can be nullptr, but in that case the script
  // execution will fail if it reaches an external action. If present,
  // |action_extension_delegate| instance must outlive the
  // |ExternalScriptController|.
  virtual std::unique_ptr<ExternalScriptController>
  CreateExternalScriptController(
      content::WebContents* web_contents,
      ExternalActionDelegate* action_extension_delegate) = 0;

 protected:
  AutofillAssistant() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_H_
