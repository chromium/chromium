// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_install_finalizer.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/launch_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_finalizer_utils.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_registrar.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut_mac.h"
#endif

namespace extensions {

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
      weak_ptr_factory_.GetWeakPtr(), web_app_info.app_url, launch_type,
      options.locally_installed, std::move(callback), crx_installer));

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

  crx_installer->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::FinalizeFallbackInstallAfterSync(
    const web_app::AppId& app_id,
    InstallFinalizedCallback callback) {
  // TODO(crbug.com/1018630): Install synced bookmark apps using a freshly
  // fetched manifest instead of sync data.
  NOTREACHED();
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
      web_app::GenerateAppIdFromURL(web_app_info.app_url);

  const Extension* existing_extension = GetEnabledExtension(expected_app_id);
  if (!existing_extension) {
    DCHECK(ExtensionRegistry::Get(profile_)->GetInstalledExtension(
        expected_app_id));
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
                     std::move(callback), crx_installer));
  crx_installer->InitializeCreationFlagsForUpdate(existing_extension,
                                                  Extension::NO_FLAGS);
  crx_installer->set_install_source(existing_extension->location());

  crx_installer->InstallWebApp(web_app_info);
}

void BookmarkAppInstallFinalizer::UninstallExternalWebApp(
    const GURL& app_url,
    web_app::ExternalInstallSource external_install_source,
    UninstallWebAppCallback callback) {
  // Bookmark apps don't support app installation from different sources.
  // |external_install_source| is ignored here.
  base::Optional<web_app::AppId> app_id =
      externally_installed_app_prefs_.LookupAppId(app_url);
  if (!app_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; No corresponding extension for url.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false));
    return;
  }

  UninstallExtension(*app_id, std::move(callback));
}

bool BookmarkAppInstallFinalizer::CanUserUninstallFromSync(
    const web_app::AppId& app_id) const {
  const Extension* app = GetEnabledExtension(app_id);
  DCHECK(app);
  return extensions::ExtensionSystem::Get(profile_)
      ->management_policy()
      ->UserMayModifySettings(app, nullptr);
}

void BookmarkAppInstallFinalizer::UninstallWebAppFromSyncByUser(
    const web_app::AppId& app_id,
    UninstallWebAppCallback callback) {
  // Bookmark apps don't support app installation from different sources.
  // Uninstall extension completely:
  UninstallExtension(app_id, std::move(callback));
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

bool BookmarkAppInstallFinalizer::CanRevealAppShim() const {
#if defined(OS_MACOSX)
  return true;
#else   // defined(OS_MACOSX)
  return false;
#endif  // !defined(OS_MACOSX)
}

void BookmarkAppInstallFinalizer::RevealAppShim(const web_app::AppId& app_id) {
  DCHECK(CanRevealAppShim());
#if defined(OS_MACOSX)
  const Extension* app = GetEnabledExtension(app_id);
  DCHECK(app);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          ::switches::kDisableHostedAppShimCreation)) {
    web_app::RevealAppShimInFinderForApp(profile_, app);
  }
#endif  // defined(OS_MACOSX)
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
    const GURL& app_url,
    LaunchType launch_type,
    bool is_locally_installed,
    InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
    return;
  }

  const Extension* extension = crx_installer->extension();
  DCHECK(extension);

  if (extension != GetEnabledExtension(extension->id())) {
    LOG(ERROR) << "Installed extension was disabled: "
               << ExtensionPrefs::Get(profile_)->GetDisableReasons(
                      extension->id());
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kWebAppDisabled);
    return;
  }

  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(extension), app_url);

  SetLaunchType(profile_, extension->id(), launch_type);

  SetBookmarkAppIsLocallyInstalled(profile_, extension, is_locally_installed);

  registrar().NotifyWebAppInstalled(extension->id());

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccessNewInstall);
}

void BookmarkAppInstallFinalizer::OnExtensionUpdated(
    const web_app::AppId& expected_app_id,
    InstallFinalizedCallback callback,
    scoped_refptr<CrxInstaller> crx_installer,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(web_app::AppId(),
                            web_app::InstallResultCode::kFailedUnknownReason);
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

  std::move(callback).Run(extension->id(),
                          web_app::InstallResultCode::kSuccessAlreadyInstalled);
}

}  // namespace extensions
