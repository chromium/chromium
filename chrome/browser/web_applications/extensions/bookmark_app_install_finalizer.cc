// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/optional.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_prefs_utils.h"
#include "chrome/browser/web_applications/components/web_app_provider_base.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_finalizer_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

namespace extensions {

static constexpr char kInstallResultExtensionErrorHistogramName[] =
    "Webapp.InstallResultExtensionError.System.Profiles";
static constexpr char kInstallResultExtensionDisabledReasonHistogramName[] =
    "Webapp.InstallResultExtensionDisabledReason.System.Profiles";

BookmarkAppInstallFinalizer::BookmarkAppInstallFinalizer(Profile* profile)
    : externally_installed_app_prefs_(profile->GetPrefs()), profile_(profile) {
  crx_installer_factory_ = base::BindRepeating([](Profile* profile) {
    ExtensionService* extension_service =
        ExtensionSystem::Get(profile)->extension_service();
    DCHECK(extension_service);
    return CrxInstaller::CreateSilent(extension_service);
  });
}

BookmarkAppInstallFinalizer::~BookmarkAppInstallFinalizer() = default;

void BookmarkAppInstallFinalizer::FinalizeInstall(
    const WebApplicationInfo& web_app_info,
    const FinalizeOptions& options,
    InstallFinalizedCallback callback) {
  scoped_refptr<CrxInstaller> crx_installer =
      crx_installer_factory_.Run(profile_);

  extensions::LaunchType launch_type =
      web_app_info.open_as_window ? LAUNCH_TYPE_WINDOW : LAUNCH_TYPE_REGULAR;

  crx_installer->set_installer_callback(base::BindOnce(
      &BookmarkAppInstallFinalizer::OnExtensionInstalled,
      weak_ptr_factory_.GetWeakPtr(), web_app_info.start_url, launch_type,
      web_app_info.enable_experimental_tabbed_window, options.locally_installed,
      options.install_source == WebappInstallSource::SYSTEM_DEFAULT,
      std::move(callback), crx_installer));

  switch (options.install_source) {
      // TODO(nigeltao/ortuno): should these two cases lead to different
      // Manifest::Location values: INTERNAL vs EXTERNAL_PREF_DOWNLOAD?
    case WebappInstallSource::INTERNAL_DEFAULT:
    case WebappInstallSource::EXTERNAL_DEFAULT:
      crx_installer->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
      // CrxInstaller::InstallWebApp will OR the creation flags with
      // FROM_BOOKMARK.
      crx_installer->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case WebappInstallSource::EXTERNAL_POLICY:
      crx_installer->set_install_source(Manifest::EXTERNAL_POLICY_DOWNLOAD);
      break;
    case WebappInstallSource::SYSTEM_DEFAULT:
      // System Apps are considered EXTERNAL_COMPONENT as they are downloaded
      // from the WebUI they point to. COMPONENT seems like the more correct
      // value, but usages (icon loading, filesystem cleanup), are tightly
      // coupled to this value, making it unsuitable.
      crx_installer->set_install_source(Manifest::EXTERNAL_COMPONENT);
      // InstallWebApp will OR the creation flags with FROM_BOOKMARK.
      crx_installer->set_creation_flags(Extension::WAS_INSTALLED_BY_DEFAULT);
      break;
    case WebappInstallSource::ARC:
      // Ensure that WebApk is not synced. There is some mechanism to propagate
      // the local source of data in place of usual extension sync.
      crx_installer->set_install_source(Manifest::EXTERNAL_PREF_DOWNLOAD);
      break;
    case WebappInstallSource::COUNT:
      NOTREACHED();
      break;
    default:
      // All other install sources mean user-installed app. Do nothing.
      break;
  }

  const web_app::AppId app_id =
      web_app::GenerateAppIdFromURL(web_app_info.start_url);
  web_app::UpdateIntWebAppPref(profile_->GetPrefs(), app_id,
                               web_app::kLatestWebAppInstallSource,
                               static_cast<int>(options.install_source));
  crx_installer->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::FinalizeUninstallAfterSync(
    const web_app::AppId& app_id,
    UninstallWebAppCallback callback) {
  // Used only by the new USS-based sync system.
  NOTREACHED();
}

void BookmarkAppInstallFinalizer::FinalizeUpdate(
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback) {
  web_app::AppId expected_app_id =
      web_app::GenerateAppIdFromURL(web_app_info.start_url);

  const Extension* existing_extension = GetEnabledExtension(expected_app_id);
  if (!existing_extension) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kWebAppDisabled);
    return;
  }
  DCHECK(existing_extension->from_bookmark());

  scoped_refptr<CrxInstaller> crx_installer =
      crx_installer_factory_.Run(profile_);
  crx_installer->set_installer_callback(
      base::BindOnce(&BookmarkAppInstallFinalizer::OnExtensionUpdated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(expected_app_id),
                     existing_extension->short_name(), web_app_info,
                     std::move(callback), crx_installer));
  crx_installer->InitializeCreationFlagsForUpdate(existing_extension,
                                                  Extension::NO_FLAGS);
  crx_installer->set_install_source(existing_extension->location());

  crx_installer->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::UninstallExternalWebApp(
    const web_app::AppId& app_id,
    web_app::ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  // Bookmark apps don't support app installation from different sources.
  // |external_install_source| is ignored here.
  UninstallExtension(app_id, std::move(callback));
}

bool BookmarkAppInstallFinalizer::CanUserUninstallFromSync(
    const web_app::AppId& app_id) const {
  // Bookmark apps don't support app installation from different sources.
  // The old system uninstalls extension completely, the implementation is
  // the same:
  return CanUserUninstallExternalApp(app_id);
}

void BookmarkAppInstallFinalizer::UninstallWebAppFromSyncByUser(
    const web_app::AppId& app_id,
    UninstallWebAppCallback callback) {
  // Bookmark apps don't support app installation from different sources.
  // Uninstall extension completely:
  UninstallExtension(app_id, std::move(callback));
}

bool BookmarkAppInstallFinalizer::CanUserUninstallExternalApp(
    const web_app::AppId& app_id) const {
  const Extension* app = GetEnabledExtension(app_id);
  return app ? extensions::ExtensionSystem::Get(profile_)
                   ->management_policy()
                   ->UserMayModifySettings(app, nullptr)
             : false;
}

void BookmarkAppInstallFinalizer::UninstallExternalAppByUser(
    const web_app::AppId& app_id,
    UninstallWebAppCallback callback) {
  // Bookmark apps don't support app installation from different sources.
  // Uninstall extension completely:
  UninstallExtension(app_id, std::move(callback));
}

bool BookmarkAppInstallFinalizer::WasExternalAppUninstalledByUser(
    const web_app::AppId& app_id) const {
  return ExtensionPrefs::Get(profile_)->IsExternalExtensionUninstalled(app_id);
}

void BookmarkAppInstallFinalizer::UninstallExtension(
    const web_app::AppId& app_id,
    UninstallWebAppCallback callback) {
  if (!GetEnabledExtension(app_id)) {
    LOG(WARNING) << "Couldn't uninstall app " << app_id
                 << "; Extension not installed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  base::string16 error;
  bool uninstalled =
      ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
          app_id, UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION, &error);

  if (!uninstalled) {
    LOG(WARNING) << "Couldn't uninstall app " << app_id << ". " << error;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), uninstalled));
}

void BookmarkAppInstallFinalizer::SetCrxInstallerFactoryForTesting(
    CrxInstallerFactory crx_installer_factory) {
  crx_installer_factory_ = crx_installer_factory;
}

const Extension* BookmarkAppInstallFinalizer::GetEnabledExtension(
    const web_app::AppId& app_id) const {
  const Extension* app =
      ExtensionRegistry::Get(profile_)->enabled_extensions().GetByID(app_id);
  return app;
}

void BookmarkAppInstallFinalizer::OnExtensionInstalled(
    const GURL& start_url,
    LaunchType launch_type,
    bool enable_experimental_tabbed_window,
    bool is_locally_installed,
    bool is_system_app,
    InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    if (is_system_app) {
      std::string extension_install_error_histogram_name =
          std::string(kInstallResultExtensionErrorHistogramName) + "." +
          web_app::GetProfileCategoryForLogging(profile_);
      base::UmaHistogramEnumeration(extension_install_error_histogram_name,
                                    error.value().detail());
    }
    std::move(callback).Run(
        web_app::AppId(),
        web_app::InstallResultCode::kBookmarkExtensionInstallError);
    return;
  }

  const Extension* extension = crx_installer->extension();
  DCHECK(extension);

  if (extension != GetEnabledExtension(extension->id())) {
    int extension_disabled_reasons =
        ExtensionPrefs::Get(profile_)->GetDisableReasons(extension->id());
    LOG(ERROR) << "Installed extension was disabled: "
               << extension_disabled_reasons;
    if (is_system_app) {
      std::string extension_disabled_reason_histogram_name =
          std::string(kInstallResultExtensionDisabledReasonHistogramName) +
          "." + web_app::GetProfileCategoryForLogging(profile_);
      base::UmaHistogramSparse(extension_disabled_reason_histogram_name,
                               extension_disabled_reasons);
    }
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kWebAppDisabled);
    return;
  }

  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), start_url);

  SetLaunchType(profile_, extension->id(), launch_type);

  SetBookmarkAppIsLocallyInstalled(profile_, extension, is_locally_installed);

  if (!is_legacy_finalizer()) {
    registry_controller().SetExperimentalTabbedWindowMode(
        extension->id(), enable_experimental_tabbed_window,
        /*is_user_action=*/false);
    registrar().NotifyWebAppInstalled(extension->id());
  }

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccessNewInstall);
}

void BookmarkAppInstallFinalizer::OnExtensionUpdated(
    const web_app::AppId& expected_app_id,
    const std::string& old_name,
    const WebApplicationInfo& web_app_info,
    InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(
        web_app::AppId(),
        web_app::InstallResultCode::kBookmarkExtensionInstallError);
    return;
  }

  const Extension* extension = crx_installer->extension();
  DCHECK(extension);
  DCHECK_EQ(extension->id(), expected_app_id);

  if (extension != GetEnabledExtension(extension->id())) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kWebAppDisabled);
    return;
  }

  if (!is_legacy_finalizer()) {
    web_app::WebAppProviderBase::GetProviderBase(profile_)
        ->os_integration_manager()
        .UpdateOsHooks(extension->id(), old_name, web_app_info);
    registrar().NotifyWebAppManifestUpdated(extension->id(), old_name);
  }
  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccessAlreadyInstalled);
}

}  // namespace extensions
