// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/bad_flags_prompt.h"

#include <algorithm>
#include <string>
#include <string_view>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "cc/base/switches.h"
#include "chrome/browser/infobars/simple_alert_infobar_creator.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/history_clusters/core/file_clustering_backend.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/media_router/common/providers/cast/certificate/switches.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/translate/core/common/translate_switches.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/switches.h"
#include "google_apis/gaia/gaia_switches.h"
#include "gpu/config/gpu_switches.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "sandbox/policy/switches.h"
#include "services/device/public/cpp/hid/hid_switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/resource/scoped_startup_resource_bundle.h"
#include "ui/views/views_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/flags/bad_flags_snackbar_manager.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#else
#include "chrome/browser/ui/browser.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/constants/chromeos_features.h"
#endif

namespace {

#if !BUILDFLAG(IS_ANDROID)
// Dangerous command line flags for which to display a warning that "stability
// and security will suffer".
const char* const kBadFlags[] = {
    // These flags allow redirecting user traffic.
    network::switches::kHostResolverRules,
    switches::kHostRules,

    // These flags disable sandbox-related security.
    sandbox::policy::switches::kDisableGpuSandbox,
    sandbox::policy::switches::kDisableSeccompFilterSandbox,
    sandbox::policy::switches::kDisableSetuidSandbox,
    sandbox::policy::switches::kNoSandbox,
#if BUILDFLAG(IS_WIN)
    sandbox::policy::switches::kAllowThirdPartyModules,
#endif
    switches::kDisableSiteIsolation,
    switches::kDisableWebSecurity,
    switches::kSingleProcess,

    // These flags disable or undermine the Same Origin Policy.
    translate::switches::kTranslateSecurityOrigin,

    // These flags undermine HTTPS / connection security.
    switches::kDisableWebRtcEncryption,
    switches::kIgnoreCertificateErrors,
    network::switches::kIgnoreCertificateErrorsSPKIList,

    // This flag could prevent QuotaChange events from firing or cause the event
    // to fire too often, potentially impacting web application behavior.
    switches::kQuotaChangeEventInterval,

    // These flags change the URLs that handle PII.
    switches::kGaiaUrl,
    translate::switches::kTranslateScriptURL,

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // This flag gives extensions more powers.
    extensions::switches::kExtensionsOnChromeURLs,
#endif

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
    // Speech dispatcher is buggy, it can crash and it can make Chrome freeze.
    // http://crbug.com/327295
    switches::kEnableSpeechDispatcher,
#endif

    // These flags control Blink feature state, which is not supported and is
    // intended only for use by Chromium developers.
    switches::kDisableBlinkFeatures,
    switches::kEnableBlinkFeatures,

    // This flag allows people to allowlist certain origins as secure, even
    // if they are not.
    network::switches::kUnsafelyTreatInsecureOriginAsSecure,

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
    switches::kDisableBestEffortTasks,

    // GPU sanboxing isn't implemented for the Web GPU API yet meaning it would
    // be possible to read GPU data for other Chromium processes.
    switches::kEnableUnsafeWebGPU,

    // A flag to bypass the WebHID blocklist for testing purposes.
    switches::kDisableHidBlocklist,

    // This flag tells Chrome to automatically install an Isolated Web App in
    // developer mode. The flag should contain the path to an unsigned Web
    // Bundle containing the IWA. Paths will be resolved relative to the
    // current working directory.
    switches::kInstallIsolatedWebAppFromFile,

    // This flag tells Chrome to automatically install an Isolated Web App in
    // developer mode. The flag should contain an HTTP(S) URL that all of the
    // app's requests will be proxied to.
    switches::kInstallIsolatedWebAppFromUrl,

    // Allows the specified origin to make Web Authentication API requests on
    // behalf of other origins, if a corresponding Google-internal
    // platform-level enterprise policy is also applied.
    webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,

    // When a file is specified as part of this flag, this sideloads machine
    // learning model output used by the History Clusters service and should
    // only be used for testing purposes.
    history_clusters::switches::kClustersOverrideFile,

    // This flag disables protection against potentially unintentional user
    // interaction with certain UI elements.
    views::switches::kDisableInputEventActivationProtectionForTesting,

    // This flag enables injecting synthetic input. It is meant to be used only
    // in tests and performance benchmarks. Using it could allow faking user
    // interaction across origins.
    cc::switches::kEnableGpuBenchmarking,

    // This flag enables loading a developer-signed certificate for Cast
    // streaming receivers and should only be used for testing purposes.
    cast_certificate::switches::kCastDeveloperCertificatePath,
};
#endif  // !BUILDFLAG(IS_ANDROID)

// Dangerous feature flags in about:flags for which to display a warning that
// "stability and security will suffer".
static const base::Feature* kBadFeatureFlagsInAboutFlags[] = {
    // This feature enables developer mode support for Isolated Web Apps.
    &features::kIsolatedWebAppDevMode,

#if BUILDFLAG(IS_ANDROID)
    &chrome::android::kCommandLineOnNonRooted,
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
    &chromeos::features::kBlinkExtensionDiagnostics,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

    // This flag disables security for the Page Embedded Permission Control, for
    // testing purposes. Can only be enabled via the command line.
    &blink::features::kBypassPepcSecurityForTesting,
};

void ShowBadFlagsInfoBarHelper(content::WebContents* web_contents,
                               int message_id,
                               std::string_view flag) {
  // Animating the infobar also animates the content area size which can trigger
  // a flood of page layout, compositing, texture reallocations, etc.  Do not
  // animate the infobar to reduce noise in perf benchmarks because they pass
  // --ignore-certificate-errors-spki-list.  This infobar only appears at
  // startup so the animation isn't visible to users anyway.
  CreateSimpleAlertInfoBar(
      infobars::ContentInfoBarManager::FromWebContents(web_contents),
      infobars::InfoBarDelegate::BAD_FLAGS_INFOBAR_DELEGATE, nullptr,
      l10n_util::GetStringFUTF16(message_id, base::UTF8ToUTF16(flag)),
      /*auto_expire=*/false, /*should_animate=*/false);
}

}  // namespace

void ShowBadFlagsPrompt(content::WebContents* web_contents) {
// On Android, ShowBadFlagsPrompt doesn't show the warning notification
// for flags which are not available in about:flags.
#if !BUILDFLAG(IS_ANDROID)
  for (const char* flag : kBadFlags) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(flag)) {
      ShowBadFlagsInfoBar(web_contents, IDS_BAD_FLAGS_WARNING_MESSAGE, flag);
      return;
    }
  }
#endif

  for (const base::Feature* feature : kBadFeatureFlagsInAboutFlags) {
    if (base::FeatureList::IsEnabled(*feature)) {
#if BUILDFLAG(IS_ANDROID)
      ShowBadFlagsSnackbar(web_contents, l10n_util::GetStringFUTF16(
                                             IDS_BAD_FEATURES_WARNING_MESSAGE,
                                             base::UTF8ToUTF16(feature->name)));
#else
      ShowBadFlagsInfoBarHelper(web_contents, IDS_BAD_FEATURES_WARNING_MESSAGE,
                                feature->name);
#endif
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
  ShowBadFlagsInfoBarHelper(web_contents, message_id,
                            std::string("--") + flag + switch_value);
}

void MaybeShowInvalidUserDataDirWarningDialog() {
  const base::FilePath& user_data_dir =
      chrome::GetInvalidSpecifiedUserDataDir();
  if (user_data_dir.empty()) {
    return;
  }

  startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();

  // Ensure there is an instance of ResourceBundle that is initialized for
  // localized string resource accesses.
  ui::ScopedStartupResourceBundle startup_resource_bundle;
  const std::u16string& title =
      l10n_util::GetStringUTF16(IDS_CANT_WRITE_USER_DIRECTORY_TITLE);
  const std::u16string& message = l10n_util::GetStringFUTF16(
      IDS_CANT_WRITE_USER_DIRECTORY_SUMMARY, user_data_dir.LossyDisplayName());

  // More complex dialogs cannot be shown before the earliest calls here.
  chrome::ShowWarningMessageBox(nullptr, title, message);
}
