// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_UI_HANDLER_H_

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/schema_registry.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/shell_dialogs/select_file_dialog.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry_observer.h"
#endif

struct PolicyStringMap {
  const char* key;
  int string_id;
};

class PolicyStatusProvider;

// The JavaScript message handler for the chrome://policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
#if BUILDFLAG(ENABLE_EXTENSIONS)
                        public extensions::ExtensionRegistryObserver,
#endif
                        public policy::PolicyService::Observer,
                        public policy::SchemaRegistry::Observer,
                        public ui::SelectFileDialog::Listener {
 public:
  PolicyUIHandler();
  ~PolicyUIHandler() override;


  static void AddLocalizedPolicyStrings(content::WebUIDataSource* source,
                                        const PolicyStringMap* strings,
                                        size_t count);

  static void AddCommonLocalizedStringsToSource(
      content::WebUIDataSource* source);

  // content::WebUIMessageHandler implementation.
  void RegisterMessages() override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // extensions::ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
#endif

  // policy::PolicyService::Observer implementation.
  void OnPolicyUpdated(const policy::PolicyNamespace& ns,
                       const policy::PolicyMap& previous,
                       const policy::PolicyMap& current) override;

  // policy::SchemaRegistry::Observer implementation.
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

 protected:
  virtual void AddPolicyName(const std::string& name,
                             base::DictionaryValue* names) const;

  // Send a dictionary containing the names of all known policies to the UI.
  virtual void SendPolicyNames() const;

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  // Send information about the current policy values to the UI. For each policy
  // whose value has been set, a dictionary containing the value and additional
  // metadata is sent.
  void SendPolicyValues() const;

  // Send the status of cloud policy to the UI. For each scope that has cloud
  // policy enabled (device and/or user), a dictionary containing status
  // information is sent.
  void SendStatus() const;

  void WritePoliciesToJSONFile(const base::FilePath& path) const;

  void HandleInitialized(const base::ListValue* args);
  void HandleReloadPolicies(const base::ListValue* args);
  void HandleExportPoliciesJSON(const base::ListValue* args);

  void OnRefreshPoliciesDone() const;

  policy::PolicyService* GetPolicyService() const;

  std::string device_domain_;

  scoped_refptr<ui::SelectFileDialog> export_policies_select_file_dialog_;

  // Providers that supply status dictionaries for user and device policy,
  // respectively. These are created on initialization time as appropriate for
  // the platform (Chrome OS / desktop) and type of policy that is in effect.
  std::unique_ptr<PolicyStatusProvider> user_status_provider_;
  std::unique_ptr<PolicyStatusProvider> device_status_provider_;
  std::unique_ptr<PolicyStatusProvider> machine_status_provider_;

  base::WeakPtrFactory<PolicyUIHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUIHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_UI_HANDLER_H_
