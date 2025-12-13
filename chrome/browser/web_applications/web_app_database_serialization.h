// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_

#include <memory>
#include <string>

#include "components/webapps/common/web_app_id.h"

namespace web_app {

class WebApp;
namespace proto {
class WebApp;
}  // namespace proto

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ProtoParseResult)
enum class ProtoParseResult {
  kSuccess = 0,
  kNoSyncData = 1,
  kNoStartUrlInSyncData = 2,
  kInvalidStartUrl = 3,
  kNoScope = 4,
  kInvalidScope = 5,
  kInvalidScopeWithRef = 6,
  kInvalidScopeWithQuery = 7,
  kNoRelativeManifestId = 8,
  kInvalidManifestId = 9,
  kNoUserDisplayModeInSync = 10,
  kMissingUserDisplayModeForCurrentPlatform = 11,
  kNoSources = 12,
  kNoSourcesAndNotUninstalling = 13,
  kNoName = 14,
  kNoInstallState = 15,
  kInvalidInstallState = 16,
  kMissingChromeOsData = 17,
  kHasChromeOsDataOnNonChromeOs = 18,
  kNoManifestIcons = 19,
  kInvalidFileHandlerNoActionOrLaunchType = 20,
  kInvalidFileHandlerAction = 21,
  kInvalidFileHandlerAcceptEntry = 22,
  kInvalidIconsInFileHandler = 23,
  kInvalidShareTarget = 24,
  kInvalidShareTargetAction = 25,
  kInvalidShareTargetFile = 26,
  kInvalidShortcutsMenuItemInfo = 27,
  kMoreDownloadedShortcutIconsThanInfos = 28,
  kEmptyAdditionalSearchTerm = 29,
  kInvalidProtocolHandler = 30,
  kInvalidProtocolHandlerUrl = 31,
  kEmptyAllowedLaunchProtocol = 32,
  kEmptyDisallowedLaunchProtocol = 33,
  kInvalidScopeExtension = 34,
  kOpaqueScopeExtensionOrigin = 35,
  kEmptyScopeExtensionOrigin = 36,
  kInvalidScopeExtensionScope = 37,
  kOpaqueValidatedScopeExtension = 38,
  kEmptyValidatedScopeExtensionOrigin = 39,
  kInvalidScopeExtensionValidated = 40,
  kScopeExtensionOriginMismatchWithScope = 41,
  kInvalidManifestUrl = 42,
  kInvalidInstallUrl = 43,
  kEmptyPolicyId = 44,
  kInvalidIsolationDataVersion = 45,
  kInvalidIsolationDataLocation = 46,
  kInvalidPendingUpdateInfoLocation = 47,
  kDevModeMismatchInIsolationData = 48,
  kInvalidPendingUpdateInfoVersion = 49,
  kInvalidPendingUpdateIntegrityBlockData = 50,
  kInvalidIntegrityBlockData = 51,
  kInvalidUpdateManifestUrlIwa = 52,
  kInvalidUpdateChannel = 53,
  kInvalidGeneratedIconFix = 54,
  kEmptyPendingUpdateInfo = 55,
  kMismatchedPendingUpdateInfoIcons = 56,
  kMissingDownloadedIconsForPendingUpdate = 57,
  kInvalidPendingUpdateManifestIcons = 58,
  kInvalidPendingUpdateTrustedIcons = 59,
  kInvalidDownloadedManifestIconForPendingUpdate = 60,
  kInvalidDownloadedTrustedIconForPendingUpdate = 61,
  kMissingWasIgnoredForPendingUpdate = 62,
  kInvalidParsedTrustedIcons = 63,
  kInvalidBorderlessUrlPatterns = 64,
  kInvalidInstalledBy = 65,
  kMaxValue = kInvalidInstalledBy,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webapps/enums.xml:WebAppProtoParseResult)

std::unique_ptr<WebApp> ParseWebAppProtoForTesting(const webapps::AppId& app_id,
                                                   const std::string& value);
std::unique_ptr<WebApp> ParseWebAppProto(const proto::WebApp& proto);
std::unique_ptr<proto::WebApp> WebAppToProto(const WebApp& web_app);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_DATABASE_SERIALIZATION_H_
