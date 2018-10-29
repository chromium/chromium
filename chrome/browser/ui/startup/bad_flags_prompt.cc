// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/bad_flags_prompt.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/infobars/core/simple_alert_infobar_delegate.h"
#include "components/invalidation/impl/invalidation_switches.h"
#include "components/nacl/common/buildflags.h"
#include "components/nacl/common/nacl_switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/service_manager/sandbox/switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#endif  // OS_ANDROID

namespace chrome {

namespace {

#if !defined(OS_ANDROID)
// Dangerous command line flags for which to display a warning that "stability
// and security will suffer".
static const char* kBadFlags[] = {
    network::switches::kIgnoreCertificateErrorsSPKIList,
    // These flags disable sandbox-related security.
    service_manager::switches::kDisableGpuSandbox,
    service_manager::switches::kDisableSeccompFilterSandbox,
    service_manager::switches::kDisableSetuidSandbox,
    service_manager::switches::kNoSandbox,
#if defined(OS_WIN)
    service_manager::switches::kAllowThirdPartyModules,
#endif
    switches::kDisableWebSecurity,
#if BUILDFLAG(ENABLE_NACL)
    switches::kNaClDangerousNoSandboxNonSfi,
#endif
    switches::kSingleProcess,

    // These flags disable or undermine the Same Origin Policy.
    translate::switches::kTranslateSecurityOrigin,

    // These flags undermine HTTPS / connection security.
    switches::kDisableWebRtcEncryption,
    switches::kIgnoreCertificateErrors,
    invalidation::switches::kSyncAllowInsecureXmppConnection,

    // These flags change the URLs that handle PII.
    switches::kGaiaUrl, translate::switches::kTranslateScriptURL,

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // This flag gives extensions more powers.
    extensions::switches::kExtensionsOnChromeURLs,
#endif

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
    // Speech dispatcher is buggy, it can crash and it can make Chrome freeze.
    // http://crbug.com/327295
    switches::kEnableSpeechDispatcher,
#endif

    // These flags control Blink feature state, which is not supported and is
    // intended only for use by Chromium developers.
    switches::kDisableBlinkFeatures, switches::kEnableBlinkFeatures,

    // This flag allows people to whitelist certain origins as secure, even
    // if they are not.
    switches::kUnsafelyTreatInsecureOriginAsSecure,

    // This flag allows sites to access the camera and microphone without
    // getting the user's permission.
    switches::kUseFakeUIForMediaStream,

    // This flag allows sites to access protected media identifiers without
    // getting the user's permission.
    switches::kUnsafelyAllowProtectedMediaIdentifierForDomain,

    // This flag delays execution of base::TaskPriority::BEST_EFFORT tasks until
    // shutdown. The queue of base::TaskPriority::BEST_EFFORT tasks can increase
    // memory usage. Also, while it should be possible to use Chrome almost
    // normally with this flag, it is expected that some non-visible operations
    // such as writing user data to disk, cleaning caches, reporting metrics or
    // updating components won't be performed until shutdown.
    switches::kDisableBackgroundTasks,
};
#endif  // OS_ANDROID

// Dangerous feature flags in about:flags for which to display a warning that
// "stability and security will suffer".
static const base::Feature* kBadFeatureFlagsInAboutFlags[] = {
    &features::kAllowSignedHTTPExchangeCertsWithoutExtension,
#if defined(OS_ANDROID)
    &chrome::android::kCommandLineOnNonRooted,
#endif  // OS_ANDROID
};

void ShowBadFeatureFlagsInfoBar(content::WebContents* web_contents,
                                int message_id,
                                const base::Feature* feature) {
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringFUTF16(message_id, base::UTF8ToUTF16(feature->name)),
      false);
}

}  // namespace

void ShowBadFlagsPrompt(content::WebContents* web_contents) {
// On Android, ShowBadFlagsPrompt doesn't show the warning notification
// for flags which are not available in about:flags.
#if !defined(OS_ANDROID)
  for (const char* flag : kBadFlags) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(flag)) {
      ShowBadFlagsInfoBar(web_contents, IDS_BAD_FLAGS_WARNING_MESSAGE, flag);
      return;
    }
  }
#endif  // OS_ANDROID

  for (const base::Feature* feature : kBadFeatureFlagsInAboutFlags) {
    if (base::FeatureList::IsEnabled(*feature)) {
      ShowBadFeatureFlagsInfoBar(web_contents, IDS_BAD_FEATURES_WARNING_MESSAGE,
                                 feature);
      return;
    }
  }
}

void ShowBadFlagsInfoBar(content::WebContents* web_contents,
                         int message_id,
                         const char* flag) {
  std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(flag);
  if (!switch_value.empty())
    switch_value = "=" + switch_value;
  SimpleAlertInfoBarDelegate::Create(
      InfoBarService::FromWebContents(web_contents),
      infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringFUTF16(
          message_id,
          base::UTF8ToUTF16(std::string("--") + flag + switch_value)),
      false);
}

void MaybeShowInvalidUserDataDirWarningDialog() {
  const base::FilePath& user_data_dir = GetInvalidSpecifiedUserDataDir();
  if (user_data_dir.empty())
    return;

  startup_metric_utils::SetNonBrowserUIDisplayed();

  // Ensure the ResourceBundle is initialized for string resource access.
  bool cleanup_resource_bundle = false;
  if (!ui::ResourceBundle::HasSharedInstance()) {
    cleanup_resource_bundle = true;
    std::string locale = l10n_util::GetApplicationLocale(std::string());
    const char kUserDataDirDialogFallbackLocale[] = "en-US";
    if (locale.empty())
      locale = kUserDataDirDialogFallbackLocale;
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        locale, NULL, ui::ResourceBundle::DO_NOT_LOAD_COMMON_RESOURCES);
  }

  const base::string16& title =
      l10n_util::GetStringUTF16(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
  const base::string16& message =
      l10n_util::GetStringFUTF16(IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY,
                                 user_data_dir.LossyDisplayName());

  if (cleanup_resource_bundle)
    ui::ResourceBundle::CleanupSharedInstance();

  // More complex dialogs cannot be shown before the earliest calls here.
  ShowWarningMessageBox(NULL, title, message);
}

}  // namespace chrome
