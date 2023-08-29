// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/installable/installable_logging.h"

#include <vector>

#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "components/webapps/browser/installable/installable_evaluator.h"
#include "content/public/browser/installability_error.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace webapps {

namespace {

// Error message strings corresponding to the InstallableStatusCode enum.
static const char kNotFromSecureOriginMessage[] =
    "Page is not served from a secure origin";
static const char kNoManifestMessage[] = "Page has no manifest <link> URL";
static const char kManifestEmptyMessage[] =
    "Manifest could not be fetched, is empty, or could not be parsed";
static const char kStartUrlNotValidMessage[] =
    "Manifest start URL is not valid";
static const char kManifestMissingNameOrShortNameMessage[] =
    "Manifest does not contain a 'name' or 'short_name' field";
static const char kManifestDisplayNotSupportedMessage[] =
    "Manifest 'display' property must be one of 'standalone', 'fullscreen', or "
    "'minimal-ui'";
static const char kManifestMissingSuitableIconMessage[] =
    "Manifest does not contain a suitable icon - PNG, SVG or WebP format of at "
    "least %dpx is required, the sizes attribute must be set, and the purpose "
    "attribute, if set, must include \"any\" or \"maskable\".";
static const char kNoAcceptableIconMessage[] =
    "No supplied icon is at least %dpx square in PNG, SVG or WebP format";
static const char kCannotDownloadIconMessage[] =
    "Could not download a required icon from the manifest";
static const char kNoIconAvailableMessage[] =
    "Downloaded icon was empty or corrupted";
static const char kPlatformNotSupportedOnAndroidMessage[] =
    "The specified application platform is not supported on Android";
static const char kNoIdSpecifiedMessage[] = "No Play store ID provided";
static const char kIdsDoNotMatchMessage[] =
    "The Play Store app URL and Play Store ID do not match";
static const char kAlreadyInstalledMessage[] = "The app is already installed";
static const char kUrlNotSupportedForWebApkMessage[] =
    "A URL in the manifest contains a username, password, or port";
static const char kInIncognitoMessage[] =
    "Page is loaded in an incognito window";
static const char kNotOfflineCapable[] = "Page does not work offline";
static const char kNoUrlForServiceWorker[] =
    "Could not check service worker without a 'start_url' field in the "
    "manifest";
static const char kPreferRelatedApplications[] =
    "Manifest specifies prefer_related_applications: true";
static const char kPreferRelatedApplicationsSupportedOnlyBetaStable[] =
    "prefer_related_applications is only supported on Chrome Beta and Stable "
    "channels on Android.";
static const char kManifestLocationChanged[] =
    "Manifest location changed during fetch";
static const char kManifestDisplayOverrideNotSupportedMessage[] =
    "Manifest contains 'display_override' field, and the first supported "
    "display mode must be one of 'standalone', 'fullscreen', or 'minimal-ui'";
static const char kPipelineRestarted[] =
    "Web app uninstalled so that it stops any running pipeline";

static const char kNotFromSecureOriginId[] = "not-from-secure-origin";
static const char kNoManifestId[] = "no-manifest";
static const char kManifestEmptyId[] = "manifest-empty";
static const char kStartUrlNotValidId[] = "start-url-not-valid";
static const char kManifestMissingNameOrShortNameId[] =
    "manifest-missing-name-or-short-name";
static const char kManifestDisplayNotSupportedId[] =
    "manifest-display-not-supported";
static const char kManifestMissingSuitableIconId[] =
    "manifest-missing-suitable-icon";
static const char kMinimumIconSizeInPixelsId[] = "minimum-icon-size-in-pixels";
static const char kNoAcceptableIconId[] = "no-acceptable-icon";
static const char kCannotDownloadIconId[] = "cannot-download-icon";
static const char kNoIconAvailableId[] = "no-icon-available";
static const char kPlatformNotSupportedOnAndroidId[] =
    "platform-not-supported-on-android";
static const char kNoIdSpecifiedId[] = "no-id-specified";
static const char kIdsDoNotMatchId[] = "ids-do-not-match";
static const char kAlreadyInstalledId[] = "already-installed";
static const char kUrlNotSupportedForWebApkId[] =
    "url-not-supported-for-webapk";
static const char kInIncognitoId[] = "in-incognito";
static const char kNotOfflineCapableId[] = "not-offline-capable";
static const char kNoUrlForServiceWorkerId[] = "no-url-for-service-worker";
static const char kPreferRelatedApplicationsId[] =
    "prefer-related-applications";
static const char kPreferRelatedApplicationsSupportedOnlyBetaStableId[] =
    "prefer-related-applications-only-beta-stable";
static const char kManifestLocationChangedId[] = "manifest-location-changed";
static const char kManifestDisplayOverrideNotSupportedId[] =
    "manifest-display-override-not-supported";
static const char kPipelineRestartedId[] = "pipeline-restarted";

const std::string& GetMessagePrefix() {
  static base::NoDestructor<std::string> message_prefix(
      "Site cannot be installed: ");
  return *message_prefix;
}

}  // namespace

std::string GetErrorMessage(InstallableStatusCode code) {
  std::string message;
  switch (code) {
    case NO_ERROR_DETECTED:
    // These codes are solely used for UMA reporting.
    case RENDERER_EXITING:
    case RENDERER_CANCELLED:
    case USER_NAVIGATED:
    case NO_MATCHING_SERVICE_WORKER:
    case INSUFFICIENT_ENGAGEMENT:
    case PACKAGE_NAME_OR_START_URL_EMPTY:
    case PREVIOUSLY_BLOCKED:
    case PREVIOUSLY_IGNORED:
    case SHOWING_NATIVE_APP_BANNER:
    case SHOWING_WEB_APP_BANNER:
    case FAILED_TO_CREATE_BANNER:
    case WAITING_FOR_MANIFEST:
    case WAITING_FOR_INSTALLABLE_CHECK:
    case NO_GESTURE:
    case WAITING_FOR_NATIVE_DATA:
    case SHOWING_APP_INSTALLATION_DIALOG:
    case DATA_TIMED_OUT:
    case WEBAPK_INSTALL_FAILED:
    case MANIFEST_DEPENDENT_TASK_NOT_RUN:
    case MAX_ERROR_CODE:
      break;
    case NOT_FROM_SECURE_ORIGIN:
      message = kNotFromSecureOriginMessage;
      break;
    case NO_MANIFEST:
      message = kNoManifestMessage;
      break;
    case MANIFEST_EMPTY:
      message = kManifestEmptyMessage;
      break;
    case START_URL_NOT_VALID:
      message = kStartUrlNotValidMessage;
      break;
    case MANIFEST_MISSING_NAME_OR_SHORT_NAME:
      message = kManifestMissingNameOrShortNameMessage;
      break;
    case MANIFEST_DISPLAY_NOT_SUPPORTED:
      message = kManifestDisplayNotSupportedMessage;
      break;
    case MANIFEST_MISSING_SUITABLE_ICON:
      message =
          base::StringPrintf(kManifestMissingSuitableIconMessage,
                             InstallableEvaluator::GetMinimumIconSizeInPx());
      break;
    case NO_ACCEPTABLE_ICON:
      message =
          base::StringPrintf(kNoAcceptableIconMessage,
                             InstallableEvaluator::GetMinimumIconSizeInPx());
      break;
    case CANNOT_DOWNLOAD_ICON:
      message = kCannotDownloadIconMessage;
      break;
    case NO_ICON_AVAILABLE:
      message = kNoIconAvailableMessage;
      break;
    case PLATFORM_NOT_SUPPORTED_ON_ANDROID:
      message = kPlatformNotSupportedOnAndroidMessage;
      break;
    case NO_ID_SPECIFIED:
      message = kNoIdSpecifiedMessage;
      break;
    case IDS_DO_NOT_MATCH:
      message = kIdsDoNotMatchMessage;
      break;
    case ALREADY_INSTALLED:
      message = kAlreadyInstalledMessage;
      break;
    case URL_NOT_SUPPORTED_FOR_WEBAPK:
      message = kUrlNotSupportedForWebApkMessage;
      break;
    case IN_INCOGNITO:
      message = kInIncognitoMessage;
      break;
    case NOT_OFFLINE_CAPABLE:
      message = kNotOfflineCapable;
      break;
    case NO_URL_FOR_SERVICE_WORKER:
      message = kNoUrlForServiceWorker;
      break;
    case PREFER_RELATED_APPLICATIONS:
      message = kPreferRelatedApplications;
      break;
    case PREFER_RELATED_APPLICATIONS_SUPPORTED_ONLY_BETA_STABLE:
      message = kPreferRelatedApplicationsSupportedOnlyBetaStable;
      break;
    case MANIFEST_URL_CHANGED:
      message = kManifestLocationChanged;
      break;
    case MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED:
      message = kManifestDisplayOverrideNotSupportedMessage;
      break;
    case PIPELINE_RESTARTED:
      message = kPipelineRestarted;
      break;
  }

  return message;
}

content::InstallabilityError GetInstallabilityError(
    InstallableStatusCode code) {
  content::InstallabilityError error;
  std::string error_id;
  std::vector<content::InstallabilityErrorArgument> error_arguments;
  switch (code) {
    case NO_ERROR_DETECTED:
    // These codes are solely used for UMA reporting.
    case RENDERER_EXITING:
    case RENDERER_CANCELLED:
    case USER_NAVIGATED:
    case NO_MATCHING_SERVICE_WORKER:
    case INSUFFICIENT_ENGAGEMENT:
    case PACKAGE_NAME_OR_START_URL_EMPTY:
    case PREVIOUSLY_BLOCKED:
    case PREVIOUSLY_IGNORED:
    case SHOWING_NATIVE_APP_BANNER:
    case SHOWING_WEB_APP_BANNER:
    case FAILED_TO_CREATE_BANNER:
    case WAITING_FOR_MANIFEST:
    case WAITING_FOR_INSTALLABLE_CHECK:
    case NO_GESTURE:
    case WAITING_FOR_NATIVE_DATA:
    case SHOWING_APP_INSTALLATION_DIALOG:
    case DATA_TIMED_OUT:
    case WEBAPK_INSTALL_FAILED:
    case MANIFEST_DEPENDENT_TASK_NOT_RUN:
    case MAX_ERROR_CODE:
      break;
    case NOT_FROM_SECURE_ORIGIN:
      error_id = kNotFromSecureOriginId;
      break;
    case NO_MANIFEST:
      error_id = kNoManifestId;
      break;
    case MANIFEST_EMPTY:
      error_id = kManifestEmptyId;
      break;
    case START_URL_NOT_VALID:
      error_id = kStartUrlNotValidId;
      break;
    case MANIFEST_MISSING_NAME_OR_SHORT_NAME:
      error_id = kManifestMissingNameOrShortNameId;
      break;
    case MANIFEST_DISPLAY_NOT_SUPPORTED:
      error_id = kManifestDisplayNotSupportedId;
      break;
    case MANIFEST_MISSING_SUITABLE_ICON:
      error_id = kManifestMissingSuitableIconId;
      error_arguments.emplace_back(
          kMinimumIconSizeInPixelsId,
          base::NumberToString(InstallableEvaluator::GetMinimumIconSizeInPx()));
      break;
    case NO_ACCEPTABLE_ICON:
      error_id = kNoAcceptableIconId;
      error_arguments.emplace_back(
          kMinimumIconSizeInPixelsId,
          base::NumberToString(InstallableEvaluator::GetMinimumIconSizeInPx()));
      break;
    case CANNOT_DOWNLOAD_ICON:
      error_id = kCannotDownloadIconId;
      break;
    case NO_ICON_AVAILABLE:
      error_id = kNoIconAvailableId;
      break;
    case PLATFORM_NOT_SUPPORTED_ON_ANDROID:
      error_id = kPlatformNotSupportedOnAndroidId;
      break;
    case NO_ID_SPECIFIED:
      error_id = kNoIdSpecifiedId;
      break;
    case IDS_DO_NOT_MATCH:
      error_id = kIdsDoNotMatchId;
      break;
    case ALREADY_INSTALLED:
      error_id = kAlreadyInstalledId;
      break;
    case URL_NOT_SUPPORTED_FOR_WEBAPK:
      error_id = kUrlNotSupportedForWebApkId;
      break;
    case IN_INCOGNITO:
      error_id = kInIncognitoId;
      break;
    case NOT_OFFLINE_CAPABLE:
      error_id = kNotOfflineCapableId;
      break;
    case NO_URL_FOR_SERVICE_WORKER:
      error_id = kNoUrlForServiceWorkerId;
      break;
    case PREFER_RELATED_APPLICATIONS:
      error_id = kPreferRelatedApplicationsId;
      break;
    case PREFER_RELATED_APPLICATIONS_SUPPORTED_ONLY_BETA_STABLE:
      error_id = kPreferRelatedApplicationsSupportedOnlyBetaStableId;
      break;
    case MANIFEST_URL_CHANGED:
      error_id = kManifestLocationChangedId;
      break;
    case MANIFEST_DISPLAY_OVERRIDE_NOT_SUPPORTED:
      error_id = kManifestDisplayOverrideNotSupportedId;
      break;
    case PIPELINE_RESTARTED:
      error_id = kPipelineRestartedId;
      break;
  }
  error.error_id = error_id;
  error.installability_error_arguments = error_arguments;
  return error;
}

void LogToConsole(content::WebContents* web_contents,
                  InstallableStatusCode code,
                  blink::mojom::ConsoleMessageLevel level) {
  if (!web_contents)
    return;

  std::string message = GetErrorMessage(code);

  if (message.empty())
    return;

  web_contents->GetPrimaryMainFrame()->AddMessageToConsole(
      level, GetMessagePrefix() + message);
}

}  // namespace webapps
