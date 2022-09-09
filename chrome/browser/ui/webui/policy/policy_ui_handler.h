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
#include "base/scoped_multi_source_observation.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/policy/value_provider/chrome_policies_value_provider.h"
#include "chrome/browser/policy/value_provider/policy_value_provider.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
class ExtensionPoliciesValueProvider;
#endif

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
class UpdaterStatusAndValueProvider;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class AshLacrosPolicyStackBridge;
#endif

class ChromePoliciesValueProvider;

class PrefChangeRegistrar;

namespace policy {
class PolicyStatusProvider;
}

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
                        public policy::PolicyValueProvider::Observer,
                        public ui::SelectFileDialog::Listener {
 public:
  PolicyUIHandler();

  PolicyUIHandler(const PolicyUIHandler&) = delete;
  PolicyUIHandler& operator=(const PolicyUIHandler&) = delete;

  ~PolicyUIHandler() override;

  static void AddCommonLocalizedStringsToSource(
      content::WebUIDataSource* source);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // policy::PolicyValueProvider::Observer implementation.
  void OnPolicyValueChanged() override;

 protected:
  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  base::Value::Dict GetPolicyNames();
  base::Value::Dict GetPolicyValues();

  void HandleExportPoliciesJson(const base::Value::List& args);
  void HandleListenPoliciesUpdates(const base::Value::List& args);
  void HandleReloadPolicies(const base::Value::List& args);
  void HandleCopyPoliciesJson(const base::Value::List& args);

  // Send information about the current policy values to the UI. Information is
  // sent in two parts to the UI:
  // - A dictionary containing all available policy names
  // - A dictionary containing the value and additional metadata for each
  // policy whose value has been set and the list of available policy IDs.
  // Policy values and names are sent separately because the UI displays the
  // policies that has their values set and the policies without value
  // separately.
  void SendPolicies();

  // Send the status of cloud policy to the UI.
  void SendStatus();

  // Get the status of cloud policy. For each scope that has cloud policy
  // enabled (device and/or user), a dictionary containing status information.
  base::Value::Dict GetStatusValue() const;

  // Build a JSON string of all the policies.
  std::string GetPoliciesAsJson();

  void WritePoliciesToJSONFile(const base::FilePath& path);

  scoped_refptr<ui::SelectFileDialog> export_policies_select_file_dialog_;

  // Providers that supply status dictionaries for user and device policy,
  // respectively. These are created on initialization time as appropriate for
  // the platform (Chrome OS / desktop) and type of policy that is in effect.
  std::unique_ptr<policy::PolicyStatusProvider> user_status_provider_;
  std::unique_ptr<policy::PolicyStatusProvider> device_status_provider_;
  std::unique_ptr<policy::PolicyStatusProvider> machine_status_provider_;

  std::unique_ptr<ChromePoliciesValueProvider> chrome_policies_value_provider_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<ExtensionPoliciesValueProvider>
      extension_policies_value_provider_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::unique_ptr<UpdaterStatusAndValueProvider>
      updater_status_and_value_provider_;
#endif  // BUILDFLAG(IS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // AshLacrosPolicyStackBridge fetches device policies for Lacros from Ash and
  // sends the signal to Ash to refresh policies. We will use it as device
  // policy value and status provider for Lacros.
  AshLacrosPolicyStackBridge* ash_lacros_policy_stack_bridge_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ScopedMultiSourceObservation<policy::PolicyValueProvider,
                                     policy::PolicyValueProvider::Observer>
      policy_value_provider_observations_{this};

  base::WeakPtrFactory<PolicyUIHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_POLICY_UI_HANDLER_H_
