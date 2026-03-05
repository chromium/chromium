// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"

#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/core/connectors_internals_utils.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_MAC)
#include "chrome/common/channel_info.h"
#include "components/version_info/channel.h"
#endif  // BUILDFLAG(IS_MAC)

namespace enterprise_connectors::utils {

namespace {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)

connectors_internals::mojom::KeyTrustLevel ParseTrustLevel(
    BPKUR::KeyTrustLevel trust_level) {
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_HW_KEY:
      return connectors_internals::mojom::KeyTrustLevel::HW;
    case BPKUR::CHROME_BROWSER_OS_KEY:
      return connectors_internals::mojom::KeyTrustLevel::OS;
    default:
      return connectors_internals::mojom::KeyTrustLevel::UNSPECIFIED;
  }
}

connectors_internals::mojom::KeyManagerPermanentFailure ConvertPermanentFailure(
    std::optional<DeviceTrustKeyManager::PermanentFailure> permanent_failure) {
  if (!permanent_failure) {
    return connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED;
  }

  switch (permanent_failure.value()) {
    case DeviceTrustKeyManager::PermanentFailure::kCreationUploadConflict:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          CREATION_UPLOAD_CONFLICT;
    case DeviceTrustKeyManager::PermanentFailure::kInsufficientPermissions:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          INSUFFICIENT_PERMISSIONS;
    case DeviceTrustKeyManager::PermanentFailure::kOsRestriction:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          OS_RESTRICTION;
    case DeviceTrustKeyManager::PermanentFailure::kInvalidInstallation:
      return connectors_internals::mojom::KeyManagerPermanentFailure::
          INVALID_INSTALLATION;
  }
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_ANDROID)

}  // namespace

connectors_internals::mojom::KeyInfoPtr GetKeyInfo() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)
  auto* key_manager = g_browser_process->browser_policy_connector()
                          ->chrome_browser_cloud_management_controller()
                          ->GetDeviceTrustKeyManager();
  if (key_manager) {
    auto metadata = key_manager->GetLoadedKeyMetadata();
    if (metadata) {
      if (!metadata->spki_bytes.empty()) {
        // A key was loaded successfully.
        return connectors_internals::mojom::KeyInfo::New(
            connectors_internals::mojom::KeyManagerInitializedValue::KEY_LOADED,
            connectors_internals::mojom::LoadedKeyInfo::New(
                ParseTrustLevel(metadata->trust_level),
                AlgorithmToType(metadata->algorithm),
                HashAndEncodeString(metadata->spki_bytes),
                connectors_internals::mojom::KeyUploadStatus::
                    NewSyncKeyResponseCode(
                        ToMojomValue(metadata->synchronization_response_code)),
                /*has_ssl_key=*/false),
            ConvertPermanentFailure(metadata->permanent_failure));
      }

      return connectors_internals::mojom::KeyInfo::New(
          connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
          nullptr, ConvertPermanentFailure(metadata->permanent_failure));
    }

    return connectors_internals::mojom::KeyInfo::New(
        connectors_internals::mojom::KeyManagerInitializedValue::NO_KEY,
        nullptr,
        connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED);
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) ||
        // BUILDFLAG(IS_ANDROID)
  return connectors_internals::mojom::KeyInfo::New(
      connectors_internals::mojom::KeyManagerInitializedValue::UNSUPPORTED,
      nullptr,
      connectors_internals::mojom::KeyManagerPermanentFailure::UNSPECIFIED);
}

bool CanDeleteDeviceTrustKey() {
#if BUILDFLAG(IS_MAC)
  version_info::Channel channel = chrome::GetChannel();
  return channel != version_info::Channel::STABLE &&
         channel != version_info::Channel::BETA;
#else
  // Unsupported on non-Mac platforms.
  return false;
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace enterprise_connectors::utils
