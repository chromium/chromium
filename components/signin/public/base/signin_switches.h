// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_buildflags.h"

class PrefService;

namespace switches {

// These switches should not be queried from CommandLine::HasSwitch() directly.
// Always go through the helper functions in account_consistency_method.h
// to properly take into account the state of field trials.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

// Symbols must be annotated with COMPONENT_EXPORT(SIGNIN_SWITCHES) so that they
// can be exported by the signin_switches component. This prevents issues with
// component layering.

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kCctSignInPrompt);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceSupervisedSigninWithCapabilities);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHistoryOptInEntryPoints);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHistoryOptInPromoCtaStringVariation);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHistoryOptInIph);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSkipCheckForAccountManagementOnSignin);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUnoForAuto);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUseHostedDomainForManagementCheckOnSignin);
#endif

COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kClearTokenService[];

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableBoundSessionCredentials);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsBoundSessionCredentialsEnabled(const PrefService* profile_prefs);

// This parameter is applicable only to the platforms that use DICE as an
// account consistency protocol.
enum class EnableBoundSessionCredentialsDiceSupport {
  // Device bound session credentials are enabled only in profiles that have
  // account consistency disabled (Incognito, Chrome Sign-In disabled in
  // Settings).
  kDisabled,
  // Device bound session credentials are enabled in all profiles, including
  // DICE-enabled profiles.
  kEnabled,
};
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>
    kEnableBoundSessionCredentialsDiceSupport;

COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<std::string>
    kEnableBoundSessionCredentialsExclusiveRegistrationPath;

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableChromeRefreshTokenBinding);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsChromeRefreshTokenBindingEnabled(const PrefService* profile_prefs);
#endif

// Enables a separate account-scoped storage for preferences.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnablePreferencesAccountStorage);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceDisableExtendedSyncPromos);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const char kForceFreDefaultBrowserStep[];
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceStartupSigninPromo);
#endif

// Used for the launch of the UNO model on Desktop Phase 0.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kExplicitBrowserSigninUIOnDesktop);
// Param to control whether the bubbles are dismissible by pressing on the
// avatar button.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<bool>
    kInterceptBubblesDismissibleByAvatarButton;

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsExplicitBrowserSigninUIOnDesktopEnabled();

// Requires `kExplicitBrowserSigninUIOnDesktop`.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kImprovedSigninUIOnDesktop);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsImprovedSigninUIOnDesktopEnabled();

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kImprovedSettingsUIOnDesktop);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsImprovedSettingsUIOnDesktopEnabled();

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableSnackbarInSettings);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableImprovedGuestProfileMenu);

#if BUILDFLAG(IS_IOS)

// Features to enable identities in auth error (stale token).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableIdentityInAuthError);

// Show the error badge on the identity disc in the NTP.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableErrorBadgeOnIdentityDisc);

// Features to enable using the ASWebAuthenticationSession to add accounts to
// device.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableASWebAuthenticationSession);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBatchUploadDesktop);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsBatchUploadDesktopEnabled();

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kShowEnterpriseDialogForAllManagedAccountsSignin);

// Enables users to perform an explicit signin upon installing an extension.
// After this, syncing for extensions will be enabled when in transport mode
// (when a user is signed in but has not turned on full sync).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableExtensionsExplicitBrowserSignin);

// This gates the new single-model approach where account bookmarks are stored
// in separate permanent folders in BookmarkModel. The flag controls whether
// BOOKMARKS datatype is enabled in the transport mode.
// TODO(crbug.com/40943550): Remove this.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSyncEnableBookmarksInTransportMode);

// Returns if the current browser supports an explicit sign in (signs the user
// into transport mode, as defined above) for extension access points (e.g. the
// `ExtensionInstalledBubbleView`).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsExtensionsExplicitBrowserSigninEnabled();

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kDeferWebSigninTrackerCreation);

}  // namespace switches

// TODO(crbug.com/337879458): Move switches below into the switches namespace.

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kStableDeviceId);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kVerifyRequestInitiatorForMirrorHeaders);
#endif  // BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfilesReordering);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kOutlineSilhouetteIcon);

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kIgnoreMirrorHeadersInBackgoundTabs);
#endif

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kNonDefaultGaiaOriginCheck);

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
