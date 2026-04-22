// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_

#include <stddef.h>
#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/resources/webui/mojom/policy.mojom.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/browser/promotion/promotion_eligibility_checker.h"
#endif  // !BUILDFLAG(IS_ANDROID)

class PrefChangeRegistrar;

namespace enterprise_management {
class GetUserEligiblePromotionsResponse;
}  // namespace enterprise_management

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
                        public policy::mojom::PolicyPageHandler,
                        public policy::PolicyValueAndStatusAggregator::Observer,
                        public policy::SchemaRegistry::Observer {
 public:
  // Constructs legacy WebUIMessageHandler.
  explicit PolicyUIHandler(Profile* profile);

  // Constructs mojo handler.
  PolicyUIHandler(
      mojo::PendingReceiver<policy::mojom::PolicyPageHandler> receiver,
      mojo::PendingRemote<policy::mojom::PolicyPageClient> client,
      Profile* profile);

  PolicyUIHandler(const PolicyUIHandler&) = delete;
  PolicyUIHandler& operator=(const PolicyUIHandler&) = delete;

  ~PolicyUIHandler() override;

  static void AddCommonLocalizedStringsToSource(
      content::WebUIDataSource* source);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // policy::PolicyValueAndStatusAggregator::Observer implementation.
  void OnPolicyValueAndStatusChanged() override;

  // policy::SchemaRegistry::Observer implementation.
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

  void set_web_ui_for_test(content::WebUI* web_ui) { set_web_ui(web_ui); }

  // policy::mojom::PolicyPageHandler implementation.
  void GetDebugString(GetDebugStringCallback callback) override;
  void RestartBrowser(const std::string& policies) override;
  void SetUserAffiliated(bool affiliated,
                         SetUserAffiliatedCallback callback) override;
  void GetAppliedTestPolicies(GetAppliedTestPoliciesCallback callback) override;
  void RevertLocalTestPolicies() override;
  void SetLocalTestPolicies(
      const std::string& policies,
      const std::string& profile_separation_policy_response,
      SetLocalTestPoliciesCallback callback) override;
  void GetPolicyLogs(GetPolicyLogsCallback callback) override;

#if !BUILDFLAG(IS_ANDROID)
  void CheckPromotionEligibility(
      CheckPromotionEligibilityCallback callback) override;
  void SetBannerDismissed() override;
  void RecordBannerRedirected() override;
#endif

  void GetPoliciesJson(policy::mojom::GetPoliciesReason reason,
                       GetPoliciesJsonCallback callback) override;

 private:
  void HandleListenPoliciesUpdates(const base::ListValue& args);
  void HandleReloadPolicies(const base::ListValue& args);
  void HandleSetLocalTestPolicies(const base::ListValue& args);
  void HandleRevertLocalTestPolicies(const base::ListValue& args);
  void HandleRestartBrowser(const base::ListValue& args);
  void HandleSetUserAffiliated(const base::ListValue& args);
  void HandleGetAppliedTestPolicies(const base::ListValue& args);
#if !BUILDFLAG(IS_ANDROID)
  void HandleShouldShowPromotion(const base::ListValue& args);
  void HandleSetBannerDismissed(const base::ListValue& args);
  void HandleRecordBannerRedirected(const base::ListValue& args);
#endif
  void HandleGetPoliciesJson(const base::ListValue& args);
#if !BUILDFLAG(IS_CHROMEOS)
  void HandleUploadReport(const base::ListValue& args);
#endif

  // Core logic for setting the user affiliation status for test policies.
  // This is used to simulate user affiliation for testing purposes.
  void SetUserAffiliatedImpl(bool affiliated);

  // Core logic for retrieving the currently applied local test policies as a
  // JSON string. Returns the current set of policies loaded in the
  // LocalTestPolicyProvider.
  const std::string& GetAppliedTestPoliciesImpl();

  // Core logic for setting local test policies from a JSON string.
  // This function is the core implementation for applying test policies
  // to the LocalTestPolicyProvider.
  void SetLocalTestPoliciesImpl(
      const std::string& policies,
      const std::string& profile_separation_policy_response);

  // Handler functions for chrome://policy/logs.
  void HandleGetPolicyLogs(const base::ListValue& args);

  // Send information about the current policy values to the UI. Information is
  // sent in two parts to the UI:
  // - A dictionary containing all available policy names
  // - A dictionary containing the value and additional metadata for each
  // policy whose value has been set and the list of available policy IDs.
  // Policy values and names are sent separately because the UI displays the
  // policies that has their values set and the policies without value
  // separately.
  void SendPolicies();

  // Send the current policy schema to the UI: the list of supported Chrome &
  // extension policies, and their types.
  void SendSchema();

  // Send the status of cloud policy to the UI. For each scope that has cloud
  // policy enabled (device and/or user), a dictionary containing status
  // information.
  void SendStatus();

#if !BUILDFLAG(IS_CHROMEOS)
  // Called when report has been uploaded, successfully or not.
  void OnReportUploaded(const std::string& callback_id);
#endif

#if !BUILDFLAG(IS_ANDROID)
  void OnPromotionEligibilityFetchedWebUiWrapper(base::Value callback_id,
                                                 bool response);

  void OnPromotionEligibilityFetched(
      CheckPromotionEligibilityCallback callback,
      enterprise_management::GetUserEligiblePromotionsResponse response);

  std::unique_ptr<enterprise_promotion::PromotionEligibilityChecker>
      promotion_eligibility_checker_;
#endif

  // Builds a raw JSON string representation of all the policies.
  std::string GetPoliciesJsonImpl(policy::mojom::GetPoliciesReason reason);

  std::unique_ptr<policy::PolicyValueAndStatusAggregator>
      policy_value_and_status_aggregator_;

  base::ScopedObservation<policy::PolicyValueAndStatusAggregator,
                          policy::PolicyValueAndStatusAggregator::Observer>
      policy_value_and_status_observation_{this};
  base::ScopedObservation<policy::SchemaRegistry,
                          policy::SchemaRegistry::Observer>
      schema_registry_observation_{this};

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  uint32_t reload_policies_count_ = 0;
  uint32_t export_to_json_count_ = 0;
  uint32_t copy_to_json_count_ = 0;
  uint32_t upload_report_count_ = 0;

  const mojo::Receiver<policy::mojom::PolicyPageHandler> receiver_{this};
  const mojo::Remote<policy::mojom::PolicyPageClient> client_{
      mojo::NullRemote()};

  raw_ref<Profile> profile_;

  base::WeakPtrFactory<PolicyUIHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_
