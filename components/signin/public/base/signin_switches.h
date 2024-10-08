// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
BASE_DECLARE_FEATURE(kSkipCheckForAccountManagementOnSignin);

// Feature flag to hide signin promo in settings page.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHideSettingsSignInPromo);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUseConsentLevelSigninForLegacyAccountEmailPref);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(
    kDontFallbackToDefaultImplementationInAccountManagerFacade);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kCctSignInPrompt);
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

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRestoreSignedInAccountAndSettingsFromBackup);
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

#if BUILDFLAG(IS_IOS)
// The feature that authorizes clear-cut to send log when UMA is enabled.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableClearCut);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRemoveSignedInAccountsDialog);

// Features to enable identities in auth error (stale token).
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableIdentityInAuthError);
// DEPRECATED: Please use `kEnableIdentityInAuthError`.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnableStaleIdentities);
#endif

// Pre-connectes the network socket for the Account Capabilities fetch, after
// receiving the signin response header from Gaia.
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kPreconnectAccountCapabilitiesPostSignin);
#endif

#if BUILDFLAG(IS_IOS)
// This flag enables IdentityManager to load all accounts when having no primary
// accounts. And it makes IdentityManager reloads AccountInfo when an update
// notification is sent by ChromeAccountManagerService. The data are reloaded
// from ChromeAccountManagerService instead of contacting Gaia server.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kAlwaysLoadDeviceAccounts);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kBatchUploadDesktop);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace switches

// TODO(crbug.com/337879458): Move switches below into the switches namespace.

#if BUILDFLAG(IS_CHROMEOS_ASH)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kStableDeviceId);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kDisallowManagedProfileSignout);

#if BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kVerifyRequestInitiatorForMirrorHeaders);
#endif  // BUILDFLAG(ENABLE_MIRROR) && !BUILDFLAG(IS_IOS)

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kProfilesReordering);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kOutlineSilhouetteIcon);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceSigninFlowInProfilePicker);
// Default value is false, and the URL used would be /AccountChooser.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<bool>
    kForceSigninReauthInProfilePickerUseAddSession;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
