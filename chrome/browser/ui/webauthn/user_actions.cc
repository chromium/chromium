// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/user_actions.h"

#include <string_view>
#include <vector>

#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/strcat.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_types.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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

std::tuple<bool, bool, bool, bool> AuthenticatorsAvailable(
    const std::vector<Mechanism>& mechanisms) {
  bool has_gpm = false;
  bool has_icloud = false;
  bool has_profile = false;
  bool has_win = false;
  // TODO(derinel): Add ChromeOS combinations.
  for (const auto& mech : mechanisms) {
    if (absl::holds_alternative<Mechanism::Credential>(mech.type)) {
      AuthenticatorType type =
          absl::get<Mechanism::Credential>(mech.type)->source;
      switch (type) {
        case AuthenticatorType::kEnclave:
          has_gpm = true;
          break;
        case AuthenticatorType::kTouchID:
          has_profile = true;
          break;
        case AuthenticatorType::kICloudKeychain:
          has_icloud = true;
          break;
        case AuthenticatorType::kWinNative:
          has_win = true;
          break;
        case AuthenticatorType::kChromeOS:
        case AuthenticatorType::kPhone:
        case AuthenticatorType::kChromeOSPasskeys:
        case AuthenticatorType::kOther:
          break;
      }
    } else if (absl::holds_alternative<Mechanism::Enclave>(mech.type)) {
      has_gpm = true;
    } else if (absl::holds_alternative<Mechanism::WindowsAPI>(mech.type)) {
      has_win = true;
    } else if (absl::holds_alternative<Mechanism::ICloudKeychain>(mech.type)) {
      has_icloud = true;
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

void RecordPriorityOptionShown(const std::vector<Mechanism>& mechanisms) {
  bool has_gpm = false;
  bool has_icloud = false;
  bool has_profile = false;
  bool has_win = false;
  std::tie(has_gpm, has_icloud, has_profile, has_win) =
      AuthenticatorsAvailable(mechanisms);

  std::string_view metric_to_emit;
  if (has_gpm) {
    CHECK(!has_win && !has_icloud && !has_profile);
    metric_to_emit = kGpmOnly;
  } else if (has_icloud) {
    CHECK(!has_gpm && !has_win && !has_profile);
    metric_to_emit = kICloudOnly;
  } else if (has_win) {
    CHECK(!has_gpm && !has_icloud && !has_profile);
    metric_to_emit = kWinOnly;
  } else if (has_profile) {
    CHECK(!has_gpm && !has_icloud && !has_win);
    metric_to_emit = kProfileOnly;
  }

  base::RecordAction(base::UserMetricsAction(
      base::StrCat(
          {"WebAuthn.GetAssertion.PriorityOptionShown.", metric_to_emit})
          .c_str()));
}

void RecordCancelClick() {
  base::RecordAction(base::UserMetricsAction("WebAuthn.Dialog.Cancelled"));
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

}  // namespace webauthn::user_actions
