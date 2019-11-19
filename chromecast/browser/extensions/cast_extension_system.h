// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_H_
#define CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/one_shot_event.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"

namespace base {
class FilePath;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class ValueStoreFactory;

// A simplified version of ExtensionSystem for cast_shell. Allows
// cast_shell to skip initialization of services it doesn't need.
class CastExtensionSystem : public ExtensionSystem,
                            public ExtensionRegistrar::Delegate {
 public:
  explicit CastExtensionSystem(content::BrowserContext* browser_context);
  ~CastExtensionSystem() override;

  // Loads an unpacked extension from a directory. Returns the extension on
  // success, or nullptr otherwise.
  const Extension* LoadExtension(const base::FilePath& extension_dir);

  // Load an extension from the contents of a manifest file.
  const Extension* LoadExtensionByManifest(const std::string& manifest);

  // Unload an extension.
  void UnloadExtension(const std::string& extension_id,
                       UnloadedExtensionReason reason);

  // Loads an unpacked platform app from a directory. Returns the extension on
  // success, or nullptr otherwise.
  // Currently this just calls LoadExtension, as apps are not loaded differently
  // than other extensions. Use LaunchApp() to actually launch the loaded app.
  const Extension* LoadApp(const base::FilePath& app_dir);

  // Initializes the extension system.
  void Init();

  // Launch the app with id |extension_id|.
  void LaunchApp(const std::string& extension_id);

  // KeyedService implementation:
  void Shutdown() override;

  // ExtensionSystem implementation:
  void InitForRegularProfile(bool extensions_enabled) override;
  ExtensionService* extension_service() override;
  RuntimeData* runtime_data() override;
  ManagementPolicy* management_policy() override;
  ServiceWorkerManager* service_worker_manager() override;
  SharedUserScriptMaster* shared_user_script_master() override;
  StateStore* state_store() override;
  StateStore* rules_store() override;
  scoped_refptr<ValueStoreFactory> store_factory() override;
  InfoMap* info_map() override;
  QuotaService* quota_service() override;
  AppSorting* app_sorting() override;
  void RegisterExtensionWithRequestContexts(
      const Extension* extension,
      const base::Closure& callback) override;
  void UnregisterExtensionWithRequestContexts(
      const std::string& extension_id,
      const UnloadedExtensionReason reason) override;
  const base::OneShotEvent& ready() const override;
  ContentVerifier* content_verifier() override;
  std::unique_ptr<ExtensionSet> GetDependentExtensions(
      const Extension* extension) override;
  void InstallUpdate(const std::string& extension_id,
                     const std::string& public_key,
                     const base::FilePath& unpacked_dir,
                     bool install_immediately,
                     InstallUpdateCallback install_update_callback) override;
  bool FinishDelayedInstallationIfReady(const std::string& extension_id,
                                        bool install_immediately) override;

  // ExtensionRegistrar::Delegate implementation:
  void PreAddExtension(const Extension* extension,
                       const Extension* old_extension) override;
  void PostActivateExtension(scoped_refptr<const Extension> extension) override;
  void PostDeactivateExtension(
      scoped_refptr<const Extension> extension) override;
  void LoadExtensionForReload(
      const ExtensionId& extension_id,
      const base::FilePath& path,
      ExtensionRegistrar::LoadErrorBehavior load_error_behavior) override;
  bool CanEnableExtension(const Extension* extension) override;
  bool CanDisableExtension(const Extension* extension) override;
  bool ShouldBlockExtension(const Extension* extension) override;

 private:
  void PostLoadExtension(const scoped_refptr<extensions::Extension>& extension);
  void OnExtensionRegisteredWithRequestContexts(
      scoped_refptr<Extension> extension);
  content::BrowserContext* browser_context_;  // Not owned.

  // Data to be accessed on the IO thread. Must outlive process_manager_.
  scoped_refptr<InfoMap> info_map_;

  std::unique_ptr<ServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<RuntimeData> runtime_data_;
  std::unique_ptr<QuotaService> quota_service_;
  std::unique_ptr<AppSorting> app_sorting_;
  std::unique_ptr<SharedUserScriptMaster> shared_user_script_master_;
  std::unique_ptr<ExtensionRegistrar> extension_registrar_;

  scoped_refptr<ValueStoreFactory> store_factory_;

  // Signaled when the extension system has completed its startup tasks.
  base::OneShotEvent ready_;

  base::WeakPtrFactory<CastExtensionSystem> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastExtensionSystem);
};

}  // namespace extensions

#endif  // CHROMECAST_BROWSER_EXTENSIONS_CAST_EXTENSION_SYSTEM_H_
