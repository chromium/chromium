// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/signin/public/base/signin_buildflags.h"

namespace switches {

// These switches should not be queried from CommandLine::HasSwitch() directly.
// Always go through the helper functions in account_consistency_method.h
// to properly take into account the state of field trials.

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.

#if BUILDFLAG(IS_ANDROID)
BASE_DECLARE_FEATURE(kSeedAccountsRevamp);
#endif

extern const char kClearTokenService[];

#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
BASE_DECLARE_FEATURE(kEnableBoundSessionCredentials);
bool IsBoundSessionCredentialsEnabled();

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
extern const base::FeatureParam<EnableBoundSessionCredentialsDiceSupport>
    kEnableBoundSessionCredentialsDiceSupport;
#endif

BASE_DECLARE_FEATURE(kEnableFetchingAccountCapabilities);

BASE_DECLARE_FEATURE(kForceDisableExtendedSyncPromos);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kForceStartupSigninPromo);
#endif

BASE_DECLARE_FEATURE(kTangibleSync);

BASE_DECLARE_FEATURE(kSearchEngineChoice);

BASE_DECLARE_FEATURE(kSearchEngineChoiceFre);

BASE_DECLARE_FEATURE(kSearchEngineChoiceSettingsUi);

// Used to experiment and validate the UNO model on Desktop. Not meant to be
// launched to stable for the moment, while it's still in a prototype state.
BASE_DECLARE_FEATURE(kUnoDesktop);

#if BUILDFLAG(IS_IOS)
BASE_DECLARE_FEATURE(kRemoveSignedInAccountsDialog);
#endif

}  // namespace switches

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SIGNIN_SWITCHES_H_
