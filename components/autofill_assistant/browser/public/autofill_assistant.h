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
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

namespace content {
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace autofill_assistant {

class WebsiteLoginManager;

// Abstract interface for exported services.
class AutofillAssistant {
 public:
  // The C++ (parsed) version of `BundleCapabilitiesInformationProto`.
  struct BundleCapabilitiesInformation {
    BundleCapabilitiesInformation();
    virtual ~BundleCapabilitiesInformation();
    BundleCapabilitiesInformation(const BundleCapabilitiesInformation& other);

    // The form signatures that the script may be started on.
    std::vector<autofill::FormSignature> trigger_form_signatures;
  };

  struct CapabilitiesInfo {
    CapabilitiesInfo();
    CapabilitiesInfo(
        const std::string& url,
        const base::flat_map<std::string, std::string>& script_parameters,
        const absl::optional<BundleCapabilitiesInformation>&
            bundle_capabilities_information = absl::nullopt);
    ~CapabilitiesInfo();
    CapabilitiesInfo(const CapabilitiesInfo& other);
    CapabilitiesInfo& operator=(const CapabilitiesInfo& other);

    std::string url;
    base::flat_map<std::string, std::string> script_parameters;
    // Additional information specified in the bundle that is needed prior to
    // starting the script.
    absl::optional<BundleCapabilitiesInformation>
        bundle_capabilities_information;
  };

  using GetCapabilitiesResponseCallback =
      base::OnceCallback<void(int http_status,
                              const std::vector<CapabilitiesInfo>&)>;

  virtual ~AutofillAssistant() = default;

  // Creates a hash prefix of `hash_prefix_length` for `origin` for use in
  // `GetCapabilitiesByHashPrefix`.
  static uint64_t GetHashPrefix(uint32_t hash_prefix_length,
                                const url::Origin& origin);

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

  // Returns an |HeadlessScriptController| which can be used to execute scripts
  // on the tab specified by |web_contents|, by calling
  // |HeadlessScriptController::StartScript|.
  // The returned |HeadlessScriptController| instance has to survive for the
  // duration of the execution of the script.
  // |action_extension_delegate| can be nullptr, but in that case the script
  // execution will fail if it reaches an external action. If present,
  // |action_extension_delegate| instance must outlive the
  // |HeadlessScriptController|.
  virtual std::unique_ptr<HeadlessScriptController>
  CreateHeadlessScriptController(
      content::WebContents* web_contents,
      ExternalActionDelegate* action_extension_delegate,
      WebsiteLoginManager* website_login_manager = nullptr) = 0;

 protected:
  AutofillAssistant() = default;
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PUBLIC_AUTOFILL_ASSISTANT_H_
