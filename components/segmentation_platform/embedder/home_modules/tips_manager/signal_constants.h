// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_SIGNAL_CONSTANTS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_SIGNAL_CONSTANTS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"

// Signals related to Tips should be defined below and also added to
// `kProfileSignalNames` or `kLocalSignalNames`. A signal cannot be added to
// both `kProfileSignalNames` and `kLocalSignalNames` - it must be one or the
// other.
//
// To decide the proper place for a signal, follow the guidance in
// https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/prefs/README.md:
//
// * Signals related to the device or application itself should be added as
//   local signals via `kLocalSignalNames`.
//
// * Signals related to the user profile should be added as profile signals
//   via `kProfileSignalNames`.
namespace segmentation_platform::tips_manager::signals {
// LINT.IfChange(signal_declarations)

// User has seen the Address Bar Position choice screen.
inline constexpr char kAddressBarPositionChoiceScreenDisplayed[] =
    "AddressBarPositionChoiceScreenDisplayed";

// User has used Lens.
inline constexpr char kLensUsed[] = "LensUsed";

// User has recently opened a shopping website.
inline constexpr char kOpenedShoppingWebsite[] = "OpenedShoppingWebsite";

// User has recently opened a website in another language.
inline constexpr char kOpenedWebsiteInAnotherLanguage[] =
    "OpenedWebsiteInAnotherLanguage";

// User has saved passwords.
inline constexpr char kSavedPasswords[] = "SavedPasswords";

// User has used Google Translation.
inline constexpr char kUsedGoogleTranslation[] = "UsedGoogleTranslation";

// User has used Password Autofill.
inline constexpr char kUsedPasswordAutofill[] = "UsedPasswordAutofill";

// LINT.ThenChange(//components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h:signal_registrations)

// LINT.IfChange(signal_registrations)

// Tips signals attached to a profile.
inline constexpr auto kProfileSignalNames =
    base::MakeFixedFlatSet<std::string_view>({
        signals::kLensUsed,
        signals::kOpenedShoppingWebsite,
        signals::kOpenedWebsiteInAnotherLanguage,
        signals::kSavedPasswords,
        signals::kUsedGoogleTranslation,
        signals::kUsedPasswordAutofill,
    });

// Tips signals related to the device or application itself. These
// signals are typically used for tracking local events that are not specific
// to any profile.
inline constexpr auto kLocalSignalNames =
    base::MakeFixedFlatSet<std::string_view>({
        signals::kAddressBarPositionChoiceScreenDisplayed,
    });

//  LINT.ThenChange(//components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h:signal_declarations)

}  // namespace segmentation_platform::tips_manager::signals

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_EMBEDDER_HOME_MODULES_TIPS_MANAGER_SIGNAL_CONSTANTS_H_
