// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_

#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "printing/buildflags/buildflags.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#endif

// List of chrome:// URLs to test for
//  1) TrustedTypes violations (see NoTrustedTypesViolation test).
//  2) Presence of TrustedTypes checks (see TrustedTypesEnabled test).
// Using a hardcoded list because this is used to parameterize tests,
// and test parameters cannot be generated at runtime from the
// WebUIConfigMap. The WebUIUrlBrowserTest.UrlsInTestList test validates
// that all URLs registered in the config map at runtime are added to
// one of the lists below.
static const char* const kChromeUrls[] = {
#if defined(NDEBUG)
    // TODO(crbug.com/487113801): Investigate why tests are flaky on dbg bots.
    "chrome://accessibility",
#endif
// TODO:(https://crbug.com/1439754): Flakily crashes on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://app-service-internals",
#endif
    "chrome://actor-internals",
    "chrome://actor-overlay",
    "chrome://attribution-internals",
    "chrome://autofill-ml-internals",
    "chrome://autofill-internals",

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    "chrome://batch-upload",
#endif

    "chrome://bookmarks",
    "chrome://bookmarks-side-panel.top-chrome",
    "chrome://certificate-manager",
    "chrome://chrome-finds-internals",
    "chrome://chrome-urls",
    "chrome://color-pipeline-internals",
    "chrome://comments-side-panel.top-chrome",
    "chrome://commerce-internals",
    "chrome://components",
    "chrome://connection-help",
    "chrome://connection-monitoring-detected",
    "chrome://connectors-internals",
    "chrome://crashes",
// TODO(crbug.com/40913109): Re-enable this test
#if !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
    "chrome://credits",
#endif
    "chrome://customize-chrome-side-panel.top-chrome",
    "chrome://data-sharing-internals",

#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://default-browser-modal",
#endif

    "chrome://debug-webuis-disabled",
    "chrome://device-log",
    "chrome://download-internals",
    "chrome://downloads",
    "chrome://extensions",
    "chrome://extensions-internals",
    "chrome://extensions-zero-state",
    "chrome://family-link-user-internals",
    "chrome://flags",
    "chrome://gcm-internals",
    "chrome://gpu",
    "chrome://histograms",
    "chrome://history",
    "chrome://history-clusters-internals",
    "chrome://history-clusters-side-panel.top-chrome",
    "chrome://history-side-panel.top-chrome",
    "chrome://indexeddb-internals",
    "chrome://infobar-internals",
    "chrome://inspect",
    "chrome://internals/session-service",
    "chrome://interstitials",
    "chrome://interstitials/ssl",
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_MAC)
    "chrome://linux-proxy-config",
#endif
    "chrome://location-internals",
    "chrome://local-state",
    "chrome://management",
    "chrome://media-engagement",
    "chrome://media-internals",
    "chrome://media-router-internals",
    "chrome://metrics-internals",
    "chrome://net-export",
    "chrome://net-internals",
    "chrome://network-errors",
    "chrome://new-tab-page",
    "chrome://newtab-footer",
    "chrome://new-tab-page-third-party",
    "chrome://newtab",
    "chrome://ntp-tiles-internals",
    "chrome://omnibox",
    "chrome://on-device-internals",

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    "chrome://on-device-translation-internals",
#endif

    "chrome://optimization-guide-internals",
    "chrome://password-manager",
    "chrome://password-manager-internals",
    "chrome://policy",
    "chrome://predictors",
    "chrome://prefs-internals",
    "chrome://privacy-sandbox-internals",
    "chrome://private-aggregation-internals",
    "chrome://private-ai-internals",
    "chrome://process-internals",
    "chrome://profile-internals",
    "chrome://quota-internals",
    "chrome://read-later.top-chrome",
    "chrome://regional-capabilities-internals",
    "chrome://reset-password",
    "chrome://safe-browsing",
    "chrome://saved-tab-groups-unsupported",
    "chrome://search-engine-choice",
    "chrome://serviceworker-internals",
    "chrome://segmentation-internals",
    "chrome://settings",
    "chrome://shopping-insights-side-panel.top-chrome",
    "chrome://signin-internals",
    "chrome://site-engagement",
    "chrome://suggest-internals",
    "chrome://support-tool",
    "chrome://sync-internals",
    "chrome://system",
    "chrome://tab-search.top-chrome",
    "chrome://tab-strip-internals",
    "chrome://terms",
    "chrome://topics-internals",
    "chrome://traces",
    "chrome://traces-internals",
    "chrome://tracing",
    "chrome://translate-internals",
    "chrome://ukm",
    "chrome://usb-internals",
    "chrome://user-actions",
    "chrome://user-education-internals",
    "chrome://version",
    "chrome://web-app-internals",
    "chrome://webnn-internals",
    "chrome://webrtc-internals",
    "chrome://webrtc-logs",
    "chrome://webui-gallery",

#if BUILDFLAG(ENABLE_VR)
    "chrome://webxr-internals",
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
    "chrome://whats-new",
#endif

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    "chrome://cast-feedback",
#endif

#if BUILDFLAG(IS_ANDROID)
    "chrome://explore-sites-internals",
    "chrome://internals/notifications",
    "chrome://internals/query-tiles",
    "chrome://snippets-internals",
    "chrome://webapks",
#endif

#if BUILDFLAG(IS_CHROMEOS)
    "chrome://accessory-update",
    "chrome://account-manager-error",
    "chrome://account-migration-welcome",
    "chrome://add-supervision/",
    "chrome://app-disabled",
    "chrome://camera-app/views/main.html",
    "chrome://bluetooth-pairing",
    "chrome://certificate-manager/",
    "chrome://cloud-upload",
    "chrome://connectivity-diagnostics",
    "chrome://crostini-installer",
    "chrome://cryptohome",
    "chrome://diagnostics",
    "chrome://drive-internals",
    "chrome://emoji-picker",
    "chrome://file-manager",
    "chrome://help-app",
    "chrome://manage-mirrorsync",
    "chrome://multidevice-internals",
    "chrome://multidevice-setup",
    "chrome://nearby",
    "chrome://nearby-internals",
    "chrome://network",
    "chrome://office-fallback",
    "chrome://os-feedback",
    "chrome-untrusted://os-feedback",
    "chrome://os-settings",
    "chrome://parent-access",
    "chrome://password-change",
    "chrome://personalization",
    "chrome://power",
    "chrome://print-management",
    "chrome-untrusted://projector",
    "chrome://proximity-auth/proximity_auth.html",
    "chrome://scanning",
    "chrome://set-time",
    "chrome://shimless-rma",
    "chrome://shortcut-customization",
    "chrome://slow",
    "chrome://smb-credentials-dialog",
    "chrome://smb-share-dialog",
    "chrome://urgent-password-expiry-notification",
    "chrome://sys-internals",
#endif
#if !BUILDFLAG(IS_CHROMEOS)
    "chrome://apps",
    "chrome://browser-switch",
    "chrome://browser-switch/internals",
    "chrome://profile-picker",
    "chrome://intro",
    "chrome://profile-customization/?debug",
    "chrome://signin-email-confirmation",
#endif
#if !BUILDFLAG(IS_MAC)
    "chrome://sandbox",
#endif  // !BUILDFLAG(IS_MAC)
#if !BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40772380): this test is flaky on mac.
    "chrome://bluetooth-internals",
#endif
#if BUILDFLAG(IS_WIN)
    "chrome://conflicts",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    "chrome://signin-dice-web-intercept.top-chrome/?debug",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS)
    "chrome://signout-confirmation",
#endif
    "chrome://webuijserror",
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    "chrome://print",
#endif
};

// List of chrome:// URLs that fail sanity check tests, i.e. at least
// one of the following is true:
// (1) Have a console error when loaded
// (2) Have a network (e.g. resource fetch) error when loaded
// (3) Don't use TrustedTypes
// Don't add new URLs here unless there's a strong reason for them to be
// exempted from these basic checks.
static constexpr const char* const kChromeUntestedUrls[] = {
    "chrome-untrusted://compose",
    "chrome-untrusted://data-sharing",
    "chrome-untrusted://lens",
    "chrome-untrusted://lens-overlay",
    "chrome-untrusted://ntp-microsoft-auth",
    "chrome-untrusted://print",
    "chrome-untrusted://read-anything-side-panel.top-chrome",
    "chrome://access-code-cast",
#if !defined(NDEBUG)
    // TODO(crbug.com/487113801): Investigate why tests are flaky on dbg bots.
    "chrome://accessibility",
#endif
    "chrome://app-settings",
    "chrome://constrained-test",
    "chrome://contextual-tasks",
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/40913109): Re-enable this test
    "chrome://credits",
#endif
    // TODO(crbug.com/40710256): Test failure due to excessive output.
    "chrome://discards",
    // TODO(crbug.com/487571789): TrustedTypesEnabled test is very flaky.
    "chrome://feedback",
    "chrome://glic",
    "chrome://glic-fre",
    "chrome-untrusted://glic",
    "chrome://history-sync-optin",
    // Not a valid URL; only internals/session-service is valid.
    "chrome://internals",
    // Note: Disabled because a DCHECK fires when directly visiting the URL.
    "chrome://managed-user-profile-notice",
    // TODO(crbug.com/40185163): DCHECK failure
    "chrome://memory-internals",
    "chrome://omnibox-popup.top-chrome",
    "chrome://profile-customization",
    "chrome://signin-dice-web-intercept.top-chrome",
    "chrome://signin-error",
    "chrome://skills",
    // TODO(crbug.com/40137561): Navigating to chrome://sync-confirmation and
    // quickly navigating away cause DCHECK failure.
    "chrome://sync-confirmation",
    "chrome://tab-group-home",
    // TODO(crbug.com/40137562): Navigating to chrome://tab-strip and quickly
    // navigating away cause DCHECK failure.
    "chrome://tab-strip.top-chrome",
    "chrome://updater",
    "chrome://view-cert",
    "chrome://watermark",
    "chrome://webui-browser",
    "chrome://webui-toolbar.top-chrome",
#if BUILDFLAG(IS_MAC)
    // TODO(crbug.com/40772380): this test is flaky on mac.
    "chrome://bluetooth-internals",
    // New UI only supported on Mac.
    "chrome://unexportable-keys-internals",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
    // Note: Disabled because a DCHECK fires when directly visiting the URL.
    "chrome://signin-reauth",
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT) && !BUILDFLAG(IS_CHROMEOS)
    // TODO(crbug.com/40250068): Move to list above when TrustedTypes are
    // enabled.
    "chrome://chrome-signin/?reason=5",
    "chrome://chrome-signin",
#endif
#if BUILDFLAG(IS_CHROMEOS)
    "chrome-untrusted://boca-app",
    "chrome-untrusted://camera-app",
    "chrome-untrusted://class-tools-remote-display",
    "chrome-untrusted://crosh",
    "chrome-untrusted://demo-mode-app",
    "chrome-untrusted://eche-app",
    "chrome-untrusted://file-manager",
    "chrome-untrusted://focus-mode-player",
    "chrome-untrusted://help-app",
    "chrome-untrusted://help-app-kids-magazine",
    "chrome-untrusted://mako",
    "chrome-untrusted://media-app",
    "chrome-untrusted://projector-annotator",
    "chrome-untrusted://sample-system-web-app",
    "chrome-untrusted://scanner-feedback",
    "chrome-untrusted://terminal",
    "chrome://add-supervision",
    "chrome://app-install-dialog",
    // TODO:(https://crbug.com/1439754): Flakily crashes on ChromeOS.
    "chrome://app-service-internals",
    "chrome://arc-overview-tracing",
    "chrome://arc-power-control",
    "chrome://borealis-credits",
    "chrome://borealis-installer",
    "chrome://borealis-motd",
    "chrome://camera-app",
    // TODO(crbug.com/40250068): Move to list above when TrustedTypes are
    // enabled.
    "chrome://chrome-signin",
    "chrome://class-tools-remote-display",
    "chrome://color-internals",
    // Crashes because message handler is not registered outside of the dialog
    // for confirm password change UI.
    "chrome://confirm-password-change",
    "chrome://crostini-credits",
    "chrome://device-emulator",
    "chrome://dlp-internals",
    "chrome://eche-app",
    "chrome://enterprise-reporting",
    "chrome://extended-updates-dialog",
    "chrome://files-internals",
    "chrome://floating-workspace",
    "chrome://focus-mode-media",
    "chrome://graduation",
    "chrome://growth-internals",
    "chrome://healthd-internals",
    "chrome://internet-config-dialog",
    "chrome://internet-detail-dialog",
    "chrome://kerberos-in-browser",
    "chrome://launcher-internals",
    "chrome://local-files-migration",
    "chrome://lock-network",
    "chrome://lock-reauth",
    "chrome://mall",
    "chrome://media-app",
    "chrome://mobilesetup",
    "chrome://notification-tester",
    "chrome://oobe",
    "chrome://os-credits",
    "chrome://os-print",
    // Needs html path to be valid.
    "chrome://proximity-auth",
    "chrome://recorder-app",
    "chrome://sample-system-web-app",
    "chrome://sanitize",
    "chrome://security-curtain",
    "chrome://sensor-info",
    "chrome://slow_trace",
    "chrome://status-area-internals",
    "chrome://vc-background",
    "chrome://vm",
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_URLS_FOR_TEST_H_
