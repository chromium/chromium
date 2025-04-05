// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/chrome_signout_confirmation_prompt.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

constexpr char kChromeSignoutPromptHistogramBaseName[] =
    "Signin.ChromeSignoutConfirmationPrompt.";
constexpr char kChromeSignoutPromptHistogramUnsyncedReauthVariant[] =
    "UnsyncedReauth";
constexpr char kChromeSignoutPromptHistogramUnsyncedVariant[] = "Unsynced";
constexpr char kChromeSignoutPromptHistogramNoUnsyncedVariant[] = "NoUnsynced";
constexpr char kChromeSignoutPromptHistogramSupervisedProfileVariant[] =
    "SupervisedProfile";

constexpr char kAccountExtensionsSignoutChoiceHistogramName[] =
    "Signin.Extensions.AccountExtensionsSignoutChoice";
}  // namespace

void RecordChromeSignoutConfirmationPromptMetrics(
    ChromeSignoutConfirmationPromptVariant variant,
    ChromeSignoutConfirmationChoice choice) {
  const char* histogram_variant_name =
      kChromeSignoutPromptHistogramNoUnsyncedVariant;
  switch (variant) {
    case ChromeSignoutConfirmationPromptVariant::kNoUnsyncedData:
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedData:
      histogram_variant_name = kChromeSignoutPromptHistogramUnsyncedVariant;
      break;
    case ChromeSignoutConfirmationPromptVariant::kUnsyncedDataWithReauthButton:
      histogram_variant_name =
          kChromeSignoutPromptHistogramUnsyncedReauthVariant;
      break;
    case ChromeSignoutConfirmationPromptVariant::kProfileWithParentalControls:
      histogram_variant_name =
          kChromeSignoutPromptHistogramSupervisedProfileVariant;
      break;
  }

  base::UmaHistogramEnumeration(
      base::StrCat(
          {kChromeSignoutPromptHistogramBaseName, histogram_variant_name}),
      choice);
}

void RecordAccountExtensionsSignoutChoice(
    ChromeSignoutConfirmationChoice choice,
    bool account_extensions_kept) {
  AccountExtensionsSignoutChoice extension_choice =
      AccountExtensionsSignoutChoice::kCancelSignout;
  switch (choice) {
    case ChromeSignoutConfirmationChoice::kCancelSignout:
    case ChromeSignoutConfirmationChoice::kCancelSignoutAndReauth:
      break;
    case ChromeSignoutConfirmationChoice::kSignout:
      extension_choice =
          account_extensions_kept
              ? AccountExtensionsSignoutChoice::kSignoutAccountExtensionsKept
              : AccountExtensionsSignoutChoice::
                    kSignoutAccountExtensionsUninstalled;
      break;
  }

  base::UmaHistogramEnumeration(kAccountExtensionsSignoutChoiceHistogramName,
                                extension_choice);
}
