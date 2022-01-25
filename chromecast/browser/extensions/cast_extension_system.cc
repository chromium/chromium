// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/extensions/cast_extension_system.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chromecast/browser/extensions/api/tts/tts_extension_api.h"
#include "components/services/app_service/public/mojom/types.mojom-shared.h"
#include "components/value_store/value_store_factory_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/info_map.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/quota_service.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/service_worker_manager.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/browser/user_script_manager.h"
#include "extensions/common/api/app_runtime.h"
#include "extensions/common/constants.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/background_info.h"

using content::BrowserContext;
namespace {

std::unique_ptr<base::DictionaryValue> LoadManifestFromString(
    const std::string& manifest,
    std::string* error) {
  JSONStringValueDeserializer deserializer(manifest);
  std::unique_ptr<base::Value> root(deserializer.Deserialize(nullptr, error));
  if (!root.get()) {
    if (error->empty()) {
      // If |error| is empty, then the file could not be read.
      // It would be cleaner to have the JSON reader give a specific error
      // in this case, but other code tests for a file error with
      // error->empty().  For now, be consistent.
      *error = "Failed to load manifest";
    } else {
      *error = base::StringPrintf(
          "%s  %s", extensions::manifest_errors::kManifestParseError,
          error->c_str());
    }
    return nullptr;
  }

  if (!root->is_dict()) {
    *error = "Invalid manifest file";
    return nullptr;
  }

  return base::DictionaryValue::From(std::move(root));
}

}  // namespace

namespace extensions {

CastExtensionSystem::CastExtensionSystem(BrowserContext* browser_context)
    : browser_context_(browser_context),
      store_factory_(
          new value_store::ValueStoreFactoryImpl(browser_context->GetPath())),
      weak_factory_(this) {}

CastExtensionSystem::~CastExtensionSystem() {}

const Extension* CastExtensionSystem::LoadExtensionByManifest(
    const std::string& manifest_data) {
  std::string error;
  std::unique_ptr<base::DictionaryValue> manifest =
      LoadManifestFromString(manifest_data, &error);
  if (!manifest.get()) {
    LOG(ERROR) << "Failed to load manifest: " << error;
    return nullptr;
  }

  scoped_refptr<extensions::Extension> extension(extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kCommandLine,
      *manifest, 0, std::string(), &error));
  if (!extension.get()) {
    LOG(ERROR) << "Failed to create extension: " << error;
    return nullptr;
  }

  PostLoadExtension(extension);

  return extension.get();
}

const Extension* CastExtensionSystem::LoadExtension(
    const base::FilePath& extension_dir) {
  return LoadExtension(kManifestFilename, extension_dir);
}

const Extension* CastExtensionSystem::LoadExtension(
    const base::FilePath::CharType* manifest_file,
    const base::FilePath& extension_dir) {
  // cast_shell only supports unpacked extensions.
  // NOTE: If you add packed extension support consider removing the flag
  // FOLLOW_SYMLINKS_ANYWHERE below. Packed extensions should not have symlinks.
  CHECK(base::DirectoryExists(extension_dir)) << extension_dir.AsUTF8Unsafe();
  int load_flags = Extension::FOLLOW_SYMLINKS_ANYWHERE;
  std::string load_error;
  scoped_refptr<Extension> extension = file_util::LoadExtension(
      extension_dir, manifest_file, std::string(),
      mojom::ManifestLocation::kComponent, load_flags, &load_error);
  if (!extension.get()) {
    LOG(ERROR) << "Loading extension at " << extension_dir.value()
               << " failed with: " << load_error;
    return nullptr;
  }

  // Log warnings.
  if (extension->install_warnings().size()) {
    LOG(WARNING) << "Warnings loading extension at " << extension_dir.value()
                 << ":";
    for (const auto& warning : extension->install_warnings())
      LOG(WARNING) << warning.message;
  }

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&CastExtensionSystem::PostLoadExtension,
                                base::Unretained(this), extension));

  return extension.get();
}

void CastExtensionSystem::UnloadExtension(const std::string& extension_id,
                                          UnloadedExtensionReason reason) {
  extension_registrar_->RemoveExtension(extension_id, reason);
}

void CastExtensionSystem::PostLoadExtension(
    const scoped_refptr<extensions::Extension>& extension) {
  extension_registrar_->AddExtension(extension);
}

const Extension* CastExtensionSystem::LoadApp(const base::FilePath& app_dir) {
  return LoadExtension(app_dir);
}

void CastExtensionSystem::Init() {
  extensions::ProcessManager::Get(browser_context_);

  // Prime the tts extension API.
  extensions::TtsAPI::GetFactoryInstance();

  // Inform the rest of the extensions system to start.
  ready_.Signal();
}

void CastExtensionSystem::LaunchApp(const ExtensionId& extension_id) {
  // Send the onLaunched event.
  DCHECK(ExtensionRegistry::Get(browser_context_)
             ->enabled_extensions()
             .Contains(extension_id));
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  AppRuntimeEventRouter::DispatchOnLaunchedEvent(
      browser_context_, extension, AppLaunchSource::kSourceUntracked, nullptr);
}

void CastExtensionSystem::Shutdown() {}

void CastExtensionSystem::InitForRegularProfile(bool extensions_enabled) {
  service_worker_manager_ =
      std::make_unique<ServiceWorkerManager>(browser_context_);
  quota_service_ = std::make_unique<QuotaService>();
  app_sorting_ = std::make_unique<NullAppSorting>();

  RendererStartupHelperFactory::GetForBrowserContext(browser_context_);

  user_script_manager_ = std::make_unique<UserScriptManager>(browser_context_);

  extension_registrar_ =
      std::make_unique<ExtensionRegistrar>(browser_context_, this);
}

ExtensionService* CastExtensionSystem::extension_service() {
  return nullptr;
}

ManagementPolicy* CastExtensionSystem::management_policy() {
  return nullptr;
}

ServiceWorkerManager* CastExtensionSystem::service_worker_manager() {
  return service_worker_manager_.get();
}

UserScriptManager* CastExtensionSystem::user_script_manager() {
  return user_script_manager_.get();
}

StateStore* CastExtensionSystem::state_store() {
  return nullptr;
}

StateStore* CastExtensionSystem::rules_store() {
  return nullptr;
}

StateStore* CastExtensionSystem::dynamic_user_scripts_store() {
  return nullptr;
}

scoped_refptr<value_store::ValueStoreFactory>
CastExtensionSystem::store_factory() {
  return store_factory_;
}

InfoMap* CastExtensionSystem::info_map() {
  if (!info_map_.get())
    info_map_ = base::MakeRefCounted<InfoMap>();
  return info_map_.get();
}

QuotaService* CastExtensionSystem::quota_service() {
  return quota_service_.get();
}

AppSorting* CastExtensionSystem::app_sorting() {
  return app_sorting_.get();
}

void CastExtensionSystem::RegisterExtensionWithRequestContexts(
    const Extension* extension,
    base::OnceClosure callback) {
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&InfoMap::AddExtension, info_map(),
                     base::RetainedRef(extension), base::Time::Now(), false,
                     false),
      std::move(callback));
}

void CastExtensionSystem::UnregisterExtensionWithRequestContexts(
    const std::string& extension_id) {}

const base::OneShotEvent& CastExtensionSystem::ready() const {
  return ready_;
}

bool CastExtensionSystem::is_ready() const {
  return ready_.is_signaled();
}

ContentVerifier* CastExtensionSystem::content_verifier() {
  return nullptr;
}

std::unique_ptr<ExtensionSet> CastExtensionSystem::GetDependentExtensions(
    const Extension* extension) {
  return std::make_unique<ExtensionSet>();
}

void CastExtensionSystem::InstallUpdate(
    const std::string& extension_id,
    const std::string& public_key,
    const base::FilePath& unpacked_dir,
    bool install_immediately,
    InstallUpdateCallback install_update_callback) {
  NOTREACHED();
}

void CastExtensionSystem::PerformActionBasedOnOmahaAttributes(
    const std::string& extension_id,
    const base::Value& attributes) {
  NOTREACHED();
}

bool CastExtensionSystem::FinishDelayedInstallationIfReady(
    const std::string& extension_id,
    bool install_immediately) {
  NOTREACHED();
  return false;
}

void CastExtensionSystem::OnExtensionRegisteredWithRequestContexts(
    scoped_refptr<Extension> extension) {
  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  registry->AddReady(extension);
  registry->TriggerOnReady(extension.get());
}

void CastExtensionSystem::PreAddExtension(const Extension* extension,
                                          const Extension* old_extension) {}

void CastExtensionSystem::PostActivateExtension(
    scoped_refptr<const Extension> extension) {}

void CastExtensionSystem::PostDeactivateExtension(
    scoped_refptr<const Extension> extension) {}

void CastExtensionSystem::LoadExtensionForReload(
    const ExtensionId& extension_id,
    const base::FilePath& path,
    ExtensionRegistrar::LoadErrorBehavior load_error_behavior) {}

bool CastExtensionSystem::CanEnableExtension(const Extension* extension) {
  return true;
}

bool CastExtensionSystem::CanDisableExtension(const Extension* extension) {
  return true;
}

bool CastExtensionSystem::ShouldBlockExtension(const Extension* extension) {
  return false;
}

}  // namespace extensions
