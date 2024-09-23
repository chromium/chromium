// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/user_actions.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/gfx/vector_icon_types.h"

namespace webauthn::user_actions {

namespace {

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;
using AuthenticatorType = device::AuthenticatorType;

constexpr std::string_view kGpmAndICloudAndProfile = "GpmAndICloudAndProfile";

constexpr std::string_view kGpmAndICloud = "GpmAndICloud";
constexpr std::string_view kGpmAndWin = "GpmAndWin";
constexpr std::string_view kGpmAndProfile = "GpmAndProfile";
constexpr std::string_view kICloudAndProfile = "ICloudAndProfile";

constexpr std::string_view kGpmOnly = "GpmOnly";
constexpr std::string_view kICloudOnly = "ICloudOnly";
constexpr std::string_view kWinOnly = "WinOnly";
constexpr std::string_view kProfileOnly = "ProfileOnly";

constexpr std::string_view kOthers = "Others";

enum class AuthenticatorCategory {
  kGpm,
  kICloud,
  kWindows,
  kProfile,
  kOther,
};

AuthenticatorCategory CategoryFromMechanism(const Mechanism& mechanism) {
  if (absl::holds_alternative<Mechanism::Credential>(mechanism.type)) {
    switch (absl::get<Mechanism::Credential>(mechanism.type)->source) {
      case AuthenticatorType::kEnclave:
        return AuthenticatorCategory::kGpm;
      case AuthenticatorType::kTouchID:
        return AuthenticatorCategory::kProfile;
      case AuthenticatorType::kICloudKeychain:
        return AuthenticatorCategory::kICloud;
      case AuthenticatorType::kWinNative:
        return AuthenticatorCategory::kWindows;
      case AuthenticatorType::kChromeOS:
      case AuthenticatorType::kPhone:
      case AuthenticatorType::kChromeOSPasskeys:
      case AuthenticatorType::kOther:
        return AuthenticatorCategory::kOther;
    }
  } else if (absl::holds_alternative<Mechanism::Enclave>(mechanism.type)) {
    return AuthenticatorCategory::kGpm;
  } else if (absl::holds_alternative<Mechanism::WindowsAPI>(mechanism.type)) {
    return AuthenticatorCategory::kWindows;
  } else if (absl::holds_alternative<Mechanism::ICloudKeychain>(
                 mechanism.type)) {
    return AuthenticatorCategory::kICloud;
  }

  return AuthenticatorCategory::kOther;
}

std::tuple<bool, bool, bool, bool> AuthenticatorsAvailable(
    const std::vector<Mechanism>& mechanisms) {
  bool has_gpm = false;
  bool has_icloud = false;
  bool has_profile = false;
  bool has_win = false;
  // TODO(derinel): Add ChromeOS combinations.
  for (const auto& mech : mechanisms) {
    switch (CategoryFromMechanism(mech)) {
      case AuthenticatorCategory::kGpm:
        has_gpm = true;
        break;
      case AuthenticatorCategory::kProfile:
        has_profile = true;
        break;
      case AuthenticatorCategory::kICloud:
        has_icloud = true;
        break;
      case AuthenticatorCategory::kWindows:
        has_win = true;
        break;
      case AuthenticatorCategory::kOther:
        break;
    }
  }
  return {has_gpm, has_icloud, has_profile, has_win};
}

}  // namespace

void RecordMultipleOptionsShown(const std::vector<Mechanism>& mechanisms,
                                bool is_create) {
  bool has_gpm = false;
  bool has_icloud = false;
  bool has_profile = false;
  bool has_win = false;
  std::tie(has_gpm, has_icloud, has_profile, has_win) =
      AuthenticatorsAvailable(mechanisms);

  std::string_view metric_to_emit;
  if (has_gpm && has_icloud && has_profile) {
    metric_to_emit = kGpmAndICloudAndProfile;
  } else if (has_gpm && has_icloud) {
    metric_to_emit = kGpmAndICloud;
  } else if (has_gpm && has_win) {
    metric_to_emit = kGpmAndWin;
  } else if (has_gpm && has_profile) {
    metric_to_emit = kGpmAndProfile;
  } else if (has_gpm) {
    metric_to_emit = kGpmOnly;
  } else if (has_icloud && has_profile) {
    metric_to_emit = kICloudAndProfile;
  } else if (has_icloud) {
    metric_to_emit = kICloudOnly;
  } else if (has_win) {
    metric_to_emit = kWinOnly;
  } else if (has_profile) {
    metric_to_emit = kProfileOnly;
  } else {
    metric_to_emit = kOthers;
  }

  std::string metric =
      is_create ? base::StrCat({"WebAuthn.MakeCredential.MultipleOptionsShown.",
                                metric_to_emit})
                : base::StrCat({"WebAuthn.GetAssertion.MultipleOptionsShown.",
                                metric_to_emit});

  base::RecordAction(base::UserMetricsAction(metric.c_str()));
}

void RecordPriorityOptionShown(const Mechanism& mechanism) {
  std::optional<std::string_view> metric_to_emit;
  switch (CategoryFromMechanism(mechanism)) {
    case AuthenticatorCategory::kGpm:
      metric_to_emit = kGpmOnly;
      break;
    case AuthenticatorCategory::kProfile:
      metric_to_emit = kProfileOnly;
      break;
    case AuthenticatorCategory::kICloud:
      metric_to_emit = kICloudOnly;
      break;
    case AuthenticatorCategory::kWindows:
      metric_to_emit = kWinOnly;
      break;
    case AuthenticatorCategory::kOther:
      break;
  }

  if (metric_to_emit.has_value()) {
    base::RecordAction(base::UserMetricsAction(
        base::StrCat(
            {"WebAuthn.GetAssertion.PriorityOptionShown.", *metric_to_emit})
            .c_str()));
  }
}

void RecordMechanismClick(const Mechanism& mech) {
  std::string_view metric_to_emit;
  switch (CategoryFromMechanism(mech)) {
    case AuthenticatorCategory::kGpm:
      metric_to_emit = kGpmOnly;
      break;
    case AuthenticatorCategory::kProfile:
      metric_to_emit = kProfileOnly;
      break;
    case AuthenticatorCategory::kICloud:
      metric_to_emit = kICloudOnly;
      break;
    case AuthenticatorCategory::kWindows:
      metric_to_emit = kWinOnly;
      break;
    case AuthenticatorCategory::kOther:
      metric_to_emit = kOthers;
      break;
  }

  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"WebAuthn.Dialog.UserSelected.", metric_to_emit}).c_str()));
}

void RecordCancelClick() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Dialog.Cancelled"));
}

void RecordAcceptClick() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Dialog.Accepted"));
}

void RecordTrustDialogShown(bool is_create) {
  std::string type = is_create ? "MakeCredential" : "GetAssertion";
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"WebAuthn.", type, ".TrustGpmDialogShown"}).c_str()));
}

void RecordCreateGpmDialogShown() {
  base::RecordAction(
      base::UserMetricsAction("WebAuthn.MakeCredential.CreateGpmDialogShown"));
}

void RecordRecoveryShown(bool is_create) {
  std::string type = is_create ? "MakeCredential" : "GetAssertion";
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"WebAuthn.", type, ".RecoverGpmShown"}).c_str()));
}

void RecordRecoveryCancelled() {
  base::RecordAction(
      base::UserMetricsAction("WebAuthn.Window.RecoverGpmCancelled"));
}

void RecordRecoverySucceeded() {
  base::RecordAction(
      base::UserMetricsAction("WebAuthn.Window.RecoverGpmSucceeded"));
}

void RecordICloudShown(bool is_create) {
  std::string type = is_create ? "MakeCredential" : "GetAssertion";
  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"WebAuthn.", type, ".ICloudShown"}).c_str()));
}

void RecordICloudCancelled() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.ICloud.Cancelled"));
}

void RecordICloudSuccess() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.ICloud.Success"));
}

void RecordGpmTouchIdDialogShown(bool is_create) {
  if (is_create) {
    base::RecordAction(base::UserMetricsAction(
        "WebAuthn.MakeCredential.GpmTouchIdDialogShown"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.GpmTouchIdDialogShown"));
  }
}

void RecordGpmPinSheetShown(bool is_credential_creation,
                            bool is_pin_creation,
                            bool is_arbitrary) {
  std::string_view webauthn_request_type =
      is_credential_creation ? "MakeCredential." : "GetAssertion.";
  std::string_view pin_mode = is_pin_creation ? "GpmCreate" : "GpmEnter";
  std::string_view pin_type = is_arbitrary ? "Arbitrary" : "";

  base::RecordAction(base::UserMetricsAction(
      base::StrCat({"WebAuthn.", webauthn_request_type, pin_mode, pin_type,
                    "PinDialogShown"})
          .c_str()));
}

void RecordGpmForgotPinClick() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Gpm.ForgotPinClicked"));
}

void RecordGpmPinOptionChangeClick() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Gpm.PinOptionChanged"));
}

void RecordGpmLockedShown() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Gpm.LockedDialogShown"));
}

void RecordGpmWinUvShown(bool is_create) {
  if (is_create) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.GpmWinUvShown"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.GpmWinUvShown"));
  }
}

void RecordGpmSuccess() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Gpm.Success"));
}

void RecordGpmFailureShown() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Gpm.Failure"));
}

void RecordChromeProfileAuthenticatorShown(bool is_create) {
  if (is_create) {
    base::RecordAction(base::UserMetricsAction(
        "WebAuthn.MakeCredential.ChromeProfileAuthenticatorShown"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "WebAuthn.GetAssertion.ChromeProfileAuthenticatorShown"));
  }
}

void RecordChromeProfileCancelled() {
  base::RecordAction(
      base::UserMetricsAction("WebAuthn.ChromeProfile.Cancelled"));
}

void RecordChromeProfileSuccess() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.ChromeProfile.Success"));
}

void RecordWindowsHelloShown(bool is_create) {
  if (is_create) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.WinHelloShown"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.WinHelloShown"));
  }
}

void RecordWindowsHelloCancelled() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.WinHello.Cancelled"));
}

void RecordWindowsHelloSuccess() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.WinHello.Success"));
}

}  // namespace webauthn::user_actions
