// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/mechanism_sorter.h"

#include <algorithm>
#include <map>
#include <numeric>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "device/fido/fido_types.h"

using Mechanism = AuthenticatorRequestDialogModel::Mechanism;

namespace {

// Enum to represent the mechanism types for internal logic.
enum class SortableMechanismType {
  kEnclavePasskey,
  kPlatformPasskey,
  kPassword,
  kOther,
};

// Helper to determine the SortableMechanismType from a Mechanism.
SortableMechanismType GetSortableMechanismType(const Mechanism& mechanism) {
  if (const auto* cred_variant =
          std::get_if<Mechanism::Credential>(&mechanism.type)) {
    if (cred_variant->value().source == device::AuthenticatorType::kEnclave) {
      return SortableMechanismType::kEnclavePasskey;
    }
    // Other AuthenticatorTypes like kTouchID, kWinNative, kICloudKeychain,
    // kPhone are considered Platform Passkeys for the logic.
    return SortableMechanismType::kPlatformPasskey;
  }
  if (std::holds_alternative<Mechanism::Password>(mechanism.type)) {
    return SortableMechanismType::kPassword;
  }
  return SortableMechanismType::kOther;
}

// Helper to get an effective timestamp for sorting and comparison.
base::Time GetEffectiveTimestamp(const Mechanism& mechanism) {
  if (const auto* password_info =
          std::get_if<Mechanism::Password>(&mechanism.type)) {
    return password_info->value().last_used_time.value_or(base::Time::Min());
  }
  if (const auto* cred_variant =
          std::get_if<Mechanism::Credential>(&mechanism.type)) {
    return cred_variant->value().last_used_time.value_or(base::Time::Min());
  }
  return base::Time::Min();
}

// Helper function to deduplicate mechanisms for each account.
// Goes through all the accounts and finds the best password, enclave passkey
// and the passkey for each account. Then selects the best credential for the
// account based on the following rules:
// Rule 1: Enclave Passkey vs Platform Passkey -> Enclave Passkey.
// Rule 2: Enclave Passkey vs Password -> Most recent.
// Rule 3: Password vs Platform Passkey -> Platform Passkey.
std::vector<Mechanism> DeduplicateMechanismsByAccount(
    const std::map<std::u16string, std::vector<const Mechanism*>>&
        grouped_by_account) {
  std::vector<Mechanism> deduplicated_mechanisms;

  for (auto const& [account_name, account_mechanisms] : grouped_by_account) {
    const Mechanism* selected_mechanism = nullptr;

    const Mechanism* best_enclave_passkey = nullptr;
    base::Time best_enclave_passkey_timestamp = base::Time::Min();

    const Mechanism* best_platform_passkey = nullptr;
    base::Time best_platform_passkey_timestamp = base::Time::Min();

    const Mechanism* best_password = nullptr;
    base::Time best_password_timestamp = base::Time::Min();

    const Mechanism* best_unknown = nullptr;

    for (const auto* mechanism : account_mechanisms) {
      SortableMechanismType type = GetSortableMechanismType(*mechanism);

      switch (type) {
        case SortableMechanismType::kEnclavePasskey: {
          base::Time current_enclave_passkey_timestamp =
              GetEffectiveTimestamp(*mechanism);
          if (best_enclave_passkey == nullptr ||
              current_enclave_passkey_timestamp >
                  best_enclave_passkey_timestamp) {
            best_enclave_passkey = mechanism;
            best_enclave_passkey_timestamp = current_enclave_passkey_timestamp;
          }
          break;
        }
        case SortableMechanismType::kPlatformPasskey: {
          base::Time current_platform_passkey_timestamp =
              GetEffectiveTimestamp(*mechanism);
          if (best_platform_passkey == nullptr ||
              current_platform_passkey_timestamp >
                  best_platform_passkey_timestamp) {
            best_platform_passkey = mechanism;
            best_platform_passkey_timestamp =
                current_platform_passkey_timestamp;
          }
          break;
        }
        case SortableMechanismType::kPassword: {
          base::Time current_password_timestamp =
              GetEffectiveTimestamp(*mechanism);
          if (best_password == nullptr ||
              best_password_timestamp < current_password_timestamp) {
            best_password = mechanism;
            best_password_timestamp = current_password_timestamp;
          }
          break;
        }
        case SortableMechanismType::kOther: {
          best_unknown = mechanism;
          break;
        }
      }
    }

    if (best_enclave_passkey) {
      // Enclave Passkey is present.
      if (best_password) {
        // Enclave Passkey vs Password: Most recent.
        if (best_password_timestamp > best_enclave_passkey_timestamp) {
          selected_mechanism = best_password;
        } else {
          selected_mechanism = best_enclave_passkey;
        }
      } else {
        // Only Enclave passkey (and possibly platform passkey, but enclave
        // passkey wins by Rule 1).
        selected_mechanism = best_enclave_passkey;
      }
    } else if (best_platform_passkey) {
      // No Enclave passkey. If platform passkey exists, it wins over password
      // by Rule 3.
      selected_mechanism = best_platform_passkey;
    } else if (best_password) {
      // Only password left.
      selected_mechanism = best_password;
    } else {
      // No passkey or password.
      selected_mechanism = best_unknown;
    }

    if (selected_mechanism) {
      deduplicated_mechanisms.emplace_back(
          std::move(const_cast<Mechanism&>(*selected_mechanism)));
    }
  }
  return deduplicated_mechanisms;
}

// Helper function to sort the deduplicated mechanisms.
std::vector<Mechanism> SortMechanisms(
    std::vector<Mechanism> deduplicated_mechanisms) {
  if (deduplicated_mechanisms.empty()) {
    return deduplicated_mechanisms;
  }

  std::vector<size_t> indices(deduplicated_mechanisms.size());
  std::iota(indices.begin(), indices.end(), 0);

  std::sort(indices.begin(), indices.end(),
            [&deduplicated_mechanisms](size_t a_idx, size_t b_idx) {
              const auto& mechanism_a = deduplicated_mechanisms[a_idx];
              const auto& mechanism_b = deduplicated_mechanisms[b_idx];
              base::Time ts_a = GetEffectiveTimestamp(mechanism_a);
              base::Time ts_b = GetEffectiveTimestamp(mechanism_b);

              // Primary sort: Most recent (descending timestamp).
              if (ts_a != ts_b) {
                return ts_a > ts_b;
              }

              // Secondary sort: Alphabetical by name if all else is equal.
              return mechanism_a.name < mechanism_b.name;
            });

  std::vector<Mechanism> actually_sorted_mechanisms;
  actually_sorted_mechanisms.reserve(deduplicated_mechanisms.size());
  for (size_t index : indices) {
    // Move from the input `deduplicated_mechanisms` vector which is a copy.
    actually_sorted_mechanisms.push_back(
        std::move(deduplicated_mechanisms[index]));
  }
  return actually_sorted_mechanisms;
}

}  // namespace

std::vector<Mechanism> MechanismSorter::ProcessMechanisms(
    std::vector<Mechanism> mechanisms, /* mechanisms is a copy */
    UIPresentation ui_presentation) {
  if (ui_presentation != UIPresentation::kModalImmediate) {
    // For non-immediate presentations, return mechanisms as is.
    // Further default sorting could be added here if needed for other modes.
    return mechanisms;
  }

  // 1. Group mechanisms by account name (Mechanism.name).
  //    Pointers in grouped_by_account point to elements in the `mechanisms`
  //    copy.
  std::map<std::u16string, std::vector<const Mechanism*>> grouped_by_account;
  for (const auto& mechanism : mechanisms) {
    grouped_by_account[mechanism.name].push_back(&mechanism);
  }

  // 2. Deduplicate mechanisms.
  //    Pass the `mechanisms` copy to allow moving from it.
  std::vector<Mechanism> deduplicated_mechanisms =
      DeduplicateMechanismsByAccount(grouped_by_account);

  // 3. Sort the deduplicated mechanisms.
  //    `deduplicated_mechanisms` is already a new vector, pass by value.
  std::vector<Mechanism> sorted_mechanisms =
      SortMechanisms(std::move(deduplicated_mechanisms));

  return sorted_mechanisms;
}
