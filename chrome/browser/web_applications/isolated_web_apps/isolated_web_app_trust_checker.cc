// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/debug/stack_trace.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_filter.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/common/content_features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/app_mode/isolated_web_app/kiosk_iwa_policy_util.h"
#include "chrome/common/chromeos/extensions/chromeos_system_extension_info.h"  // nogncheck
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {

base::flat_set<web_package::SignedWebBundleId>&
GetTrustedWebBundleIdsForTesting() {
  static base::NoDestructor<base::flat_set<web_package::SignedWebBundleId>>
      trusted_web_bundle_ids_for_testing;
  return *trusted_web_bundle_ids_for_testing;
}

#if BUILDFLAG(IS_CHROMEOS)
// Returns `true` if this Web Bundle ID is configured and currently running as
// a kiosk. Kiosk mode is configured via DeviceLocalAccounts enterprise
// policy.
base::expected<void, std::string> IsTrustedForKiosk(
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (ash::GetCurrentKioskIwaBundleId() == web_bundle_id) {
    return base::ok();
  }
  return base::unexpected("IWAs in Kiosk mode can only run in Kiosk sessions.");
}

base::expected<void, std::string> IsTrustedForShimlessRma(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (ash::IsShimlessRmaAppBrowserContext(&profile) &&
      chromeos::Is3pDiagnosticsIwaId(web_bundle_id)) {
    return base::ok();
  }
  return base::unexpected(
      "Shimless RMA IWA is only supported in Shimless Profile on "
      "ChromeOS.");
}
#endif  // BUILDFLAG(IS_CHROMEOS)

base::expected<void, std::string> IsTrustedForIwaPolicy(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (!ChromeIwaRuntimeDataProvider::GetInstance().IsManagedInstallPermitted(
          web_bundle_id.id())) {
    return base::unexpected(
        "IWA with WebAppManagement::Type::kIwaPolicy must be on the managed "
        "allowlist.");
  }
  if (std::ranges::none_of(
          profile.GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList),
          [&web_bundle_id](const base::Value& force_install_entry) {
            auto options =
                IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
                    force_install_entry);
            return options.has_value() &&
                   options->web_bundle_id() == web_bundle_id;
          })) {
    return base::unexpected(
        "IWA with WebAppManagement::Type::kIwaPolicy must be listed on the"
        "IsolatedWebAppInstallForceListPolicy.");
  }
  return base::ok();
}

base::expected<void, std::string> IsTrustedForUserInstall(
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (!ChromeIwaRuntimeDataProvider::GetInstance().GetUserInstallAllowlistData(
          web_bundle_id.id())) {
    return base::unexpected(
        "IWA with WebAppManagement::Type::kIwaUserInstalled must be on the "
        "user "
        "install allowlist.");
  }
  return base::ok();
}

base::expected<void, std::string> CheckAgainstBlocklist(
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (ChromeIwaRuntimeDataProvider::GetInstance().IsBundleBlocklisted(
          web_bundle_id.id())) {
    return base::unexpected("IWA is blocklisted.");
  }
  return base::ok();
}

base::expected<void, std::string> EnsureDevModeEnabled(Profile& profile) {
  if (!IsIwaDevModeEnabled(&profile)) {
    return base::unexpected(
        "Isolated Web App Developer Mode is not enabled or blocked by "
        "policy.");
  }
  return base::ok();
}

base::expected<WebAppManagement::Type, std::string> GetHighestPrioritySource(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id) {
  if (auto* provider = WebAppProvider::GetForWebApps(&profile)) {
    if (auto* iwa = provider->registrar_unsafe().GetAppById(
            IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
                .app_id(),
            WebAppFilter::IsIsolatedApp())) {
      return iwa->GetHighestPrioritySource();
    }
  }
  return base::unexpected(
      "Something went wrong while quering the Web App system.");
}

base::expected<void, std::string> IsTrustedForManagementType(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    WebAppManagement::Type type) {
  switch (type) {
    case WebAppManagement::Type::kIwaPolicy: {
      return IsTrustedForIwaPolicy(profile, web_bundle_id);
    }
    case WebAppManagement::Type::kIwaUserInstalled: {
      return IsTrustedForUserInstall(web_bundle_id);
    }
#if BUILDFLAG(IS_CHROMEOS)
    case WebAppManagement::Type::kKiosk: {
      return IsTrustedForKiosk(web_bundle_id);
    }
    case WebAppManagement::Type::kIwaShimlessRma: {
      return IsTrustedForShimlessRma(profile, web_bundle_id);
    }
#endif  // BUILDFLAG(IS_CHROMEOS)
    default:
      NOTREACHED();
  }
}

}  // namespace

// static
base::expected<void, std::string>
IsolatedWebAppTrustChecker::IsOperationAllowed(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool dev_mode,
    const IwaOperation& operation) {
  if (dev_mode) {
    RETURN_IF_ERROR(CheckAgainstBlocklist(web_bundle_id));
    return EnsureDevModeEnabled(profile);
  }

  if (GetTrustedWebBundleIdsForTesting().contains(web_bundle_id)) {
    return base::ok();
  }

  return std::visit(
      absl::Overload{
          [&](const IwaInstallOperation& op)
              -> base::expected<void, std::string> {
            RETURN_IF_ERROR(CheckAgainstBlocklist(web_bundle_id));
            return IsTrustedForManagementType(
                profile, web_bundle_id,
                ConvertInstallSurfaceToWebAppSource(op.source));
          },
          [&](const IwaUpdateOperation&) -> base::expected<void, std::string> {
            RETURN_IF_ERROR(CheckAgainstBlocklist(web_bundle_id));
            ASSIGN_OR_RETURN(WebAppManagement::Type type,
                             GetHighestPrioritySource(profile, web_bundle_id));
            return IsTrustedForManagementType(profile, web_bundle_id, type);
          },
          [&](const IwaMetadataReadingOperation&)
              -> base::expected<void, std::string> {
            // Metadata reading is always fine since it doesn't modify the state
            // of the system.
            return base::ok();
          }},
      operation);
}

// static
base::expected<void, std::string>
IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const NonInstalledBundleInspectionContext& context) {
  return IsOperationAllowed(profile, web_bundle_id, context.source().dev_mode(),
                            context.operation());
}

// static
base::expected<void, std::string>
IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
    Profile& profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const WebApp& iwa) {
  RETURN_IF_ERROR(CheckAgainstBlocklist(web_bundle_id));

  if (WebAppProvider::GetForWebApps(&profile)->registrar_unsafe().AppMatches(
          iwa.app_id(), WebAppFilter::IsDevModeIsolatedApp())) {
    return EnsureDevModeEnabled(profile);
  }

  // The IWA is assumed to be trusted for as long as it remains installed; it's
  // the responsibility of various per-management-type managers to ensure faulty
  // IWAs are removed.
  return base::ok();
}

void SetTrustedWebBundleIdsForTesting(  // IN-TEST
    base::flat_set<web_package::SignedWebBundleId> trusted_web_bundle_ids) {
  DCHECK(
      std::ranges::none_of(trusted_web_bundle_ids,
                           &web_package::SignedWebBundleId::is_for_proxy_mode))
      << "Cannot trust Web Bundle IDs of type ProxyMode";

  GetTrustedWebBundleIdsForTesting() =  // IN-TEST
      std::move(trusted_web_bundle_ids);
}

void AddTrustedWebBundleIdForTesting(  // IN-TEST
    const web_package::SignedWebBundleId& trusted_web_bundle_id) {
  DCHECK(!trusted_web_bundle_id.is_for_proxy_mode())
      << "Cannot trust Web Bundle IDs of type ProxyMode";

  GetTrustedWebBundleIdsForTesting().insert(trusted_web_bundle_id);  // IN-TEST
}

}  // namespace web_app
