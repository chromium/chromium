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
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/policy_value_and_status_aggregator.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"

class PrefChangeRegistrar;

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
                        public policy::PolicyValueAndStatusAggregator::Observer,
                        public policy::SchemaRegistry::Observer {
 public:
  PolicyUIHandler();

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

 private:
  void HandleExportPoliciesJson(const base::Value::List& args);
  void HandleListenPoliciesUpdates(const base::Value::List& args);
  void HandleReloadPolicies(const base::Value::List& args);
  void HandleCopyPoliciesJson(const base::Value::List& args);
  void HandleSetLocalTestPolicies(const base::Value::List& args);
  void HandleRevertLocalTestPolicies(const base::Value::List& args);
  void HandleRestartBrowser(const base::Value::List& args);
  void HandleSetUserAffiliated(const base::Value::List& args);
  void HandleGetAppliedTestPolicies(const base::Value::List& args);
  void HandleShouldShowPromotion(const base::Value::List& args);
  void HandleSetBannerDismissed(const base::Value::List& args);
#if !BUILDFLAG(IS_CHROMEOS)
  void HandleUploadReport(const base::Value::List& args);
#endif

  // Handler functions for chrome://policy/logs.
  void HandleGetPolicyLogs(const base::Value::List& args);

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

  // Build a JSON string of all the policies.
  std::string GetPoliciesAsJson();

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

  base::WeakPtrFactory<PolicyUIHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_
