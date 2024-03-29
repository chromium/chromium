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
BASE_DECLARE_FEATURE(kSeedAccountsRevamp);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kEnterprisePolicyOnSignin);

// Feature flag to hide signin promo in settings page.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kHideSettingsSignInPromo);
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
BASE_DECLARE_FEATURE(kEnableFetchingAccountCapabilities);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceDisableExtendedSyncPromos);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kForceStartupSigninPromo);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRestoreSignedInAccountAndSettingsFromBackup);
#endif

#if BUILDFLAG(IS_ANDROID)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSearchEngineChoice);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kSearchEnginePromoDialogRewrite);
#endif

// Used to experiment and validate the UNO model on Desktop. Not meant to be
// launched to stable for the moment, while it's still in a prototype state.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUnoDesktop);

// Used for the launch of the UNO model on Desktop, as well as for the later
// phases of the experiment.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kExplicitBrowserSigninUIOnDesktop);
// Param to control whether the bubbles are dismissible by pressing on the
// avatar button.
COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<bool>
    kInterceptBubblesDismissibleByAvatarButton;

enum class ExplicitBrowserSigninPhase {
  // Used to enable the changes made for the experimental feature `kUnoDesktop`
  // and for the full launch feature `kExplicitBrowserSigninUIOnDesktop`.
  kExperimental = 0,
  // Used to enable the changes made only for the full launch feature
  // `kExplicitBrowserSigninUIOnDesktop`.
  kFull = 1,
};

COMPONENT_EXPORT(SIGNIN_SWITCHES)
bool IsExplicitBrowserSigninUIOnDesktopEnabled(
    ExplicitBrowserSigninPhase phase);

// Controls the view mode for (history) sync screen.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kMinorModeRestrictionsForHistorySyncOptIn);

COMPONENT_EXPORT(SIGNIN_SWITCHES)
extern const base::FeatureParam<int> kMinorModeRestrictionsFetchDeadlineMs;
#endif

#if BUILDFLAG(IS_IOS)
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kUseSystemCapabilitiesForMinorModeRestrictions);
COMPONENT_EXPORT(SIGNIN_SWITCHES)
BASE_DECLARE_FEATURE(kRemoveSignedInAccountsDialog);
#endif

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
