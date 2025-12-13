// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_H_
#define CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_H_

#include <algorithm>
#include <map>
#include <numeric>
#include <variant>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace webauthn::sorting {

// LINT.IfChange
//
// Represents the type of credential that was selected after deduplication.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WebAuthnDeduplicatedType {
  kGpmPasskey = 0,
  kPlatformPasskey = 1,
  kPassword = 2,
  kOther = 3,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

enum class SortableCredentialType {
  kGpmPasskey,
  kPlatformPasskey,
  kPassword,
  kOther,
};

// Helper function to deduplicate credentials for each account.
// Goes through all the accounts and finds the best password, GPM passkey
// and the passkey for each account. Then selects the best credential for the
// account based on the following rules:
// Rule 1: GPM Passkey vs Platform Passkey -> GPM Passkey.
// Rule 2: GPM Passkey vs Password -> Most recent.
// Rule 3: Password vs Platform Passkey -> Platform Passkey.
template <typename T, typename Traits>
std::vector<T> DeduplicateCredentialsByAccount(
    const std::map<std::u16string, std::vector<const T*>>& grouped_by_account) {
  std::vector<T> deduplicated_credentials;

  for (auto const& [account_name, account_credentials] : grouped_by_account) {
    const T* selected_credential = nullptr;

    const T* best_gpm_passkey = nullptr;
    base::Time best_gpm_passkey_timestamp = base::Time::Min();

    const T* best_platform_passkey = nullptr;
    base::Time best_platform_passkey_timestamp = base::Time::Min();

    const T* best_password = nullptr;
    base::Time best_password_timestamp = base::Time::Min();

    const T* best_unknown = nullptr;

    for (const T* credential : account_credentials) {
      SortableCredentialType type =
          Traits::GetSortableCredentialType(*credential);

      switch (type) {
        case SortableCredentialType::kGpmPasskey: {
          base::Time current_gpm_passkey_timestamp =
              Traits::GetLastUsedTime(*credential);
          if (best_gpm_passkey == nullptr ||
              current_gpm_passkey_timestamp > best_gpm_passkey_timestamp) {
            best_gpm_passkey = credential;
            best_gpm_passkey_timestamp = current_gpm_passkey_timestamp;
          }
          break;
        }
        case SortableCredentialType::kPlatformPasskey: {
          base::Time current_platform_passkey_timestamp =
              Traits::GetLastUsedTime(*credential);
          if (best_platform_passkey == nullptr ||
              current_platform_passkey_timestamp >
                  best_platform_passkey_timestamp) {
            best_platform_passkey = credential;
            best_platform_passkey_timestamp =
                current_platform_passkey_timestamp;
          }
          break;
        }
        case SortableCredentialType::kPassword: {
          base::Time current_password_timestamp =
              Traits::GetLastUsedTime(*credential);
          if (best_password == nullptr ||
              best_password_timestamp < current_password_timestamp) {
            best_password = credential;
            best_password_timestamp = current_password_timestamp;
          }
          break;
        }
        case SortableCredentialType::kOther: {
          best_unknown = credential;
          break;
        }
      }
    }

    if (best_gpm_passkey) {
      // GPM Passkey is present.
      if (best_password) {
        // GPM Passkey vs Password: Most recent.
        if (best_password_timestamp > best_gpm_passkey_timestamp) {
          selected_credential = best_password;
        } else {
          selected_credential = best_gpm_passkey;
        }
      } else {
        // Only GPM passkey (and possibly platform passkey, but GPM
        // passkey wins by Rule 1).
        selected_credential = best_gpm_passkey;
      }
    } else if (best_platform_passkey) {
      // No GPM passkey. If platform passkey exists, it wins over password
      // by Rule 3.
      selected_credential = best_platform_passkey;
    } else if (best_password) {
      // Only password left.
      selected_credential = best_password;
    } else {
      // No passkey or password.
      selected_credential = best_unknown;
    }

    if (selected_credential) {
      if (account_credentials.size() > 1) {
        WebAuthnDeduplicatedType type_to_log;
        switch (Traits::GetSortableCredentialType(*selected_credential)) {
          case SortableCredentialType::kGpmPasskey:
            type_to_log = WebAuthnDeduplicatedType::kGpmPasskey;
            break;
          case SortableCredentialType::kPlatformPasskey:
            type_to_log = WebAuthnDeduplicatedType::kPlatformPasskey;
            break;
          case SortableCredentialType::kPassword:
            type_to_log = WebAuthnDeduplicatedType::kPassword;
            break;
          case SortableCredentialType::kOther:
            type_to_log = WebAuthnDeduplicatedType::kOther;
            break;
        }
        base::UmaHistogramEnumeration(
            "WebAuthentication.MechanismSorter.SelectedMechanismType",
            type_to_log);
      }
      deduplicated_credentials.emplace_back(
          std::move(const_cast<T&>(*selected_credential)));
    }
  }
  return deduplicated_credentials;
}

// Helper function to sort the deduplicated credentials.
template <typename T, typename Traits>
std::vector<T> SortCredentials(std::vector<T> deduplicated_credentials) {
  if (deduplicated_credentials.empty()) {
    return deduplicated_credentials;
  }

  std::vector<size_t> indices(deduplicated_credentials.size());
  std::iota(indices.begin(), indices.end(), 0);

  std::sort(indices.begin(), indices.end(),
            [&deduplicated_credentials](size_t a_idx, size_t b_idx) {
              const auto& credential_a = deduplicated_credentials[a_idx];
              const auto& credential_b = deduplicated_credentials[b_idx];
              base::Time ts_a = Traits::GetLastUsedTime(credential_a);
              base::Time ts_b = Traits::GetLastUsedTime(credential_b);

              // Primary sort: Most recent (descending timestamp).
              if (ts_a != ts_b) {
                return ts_a > ts_b;
              }

              // Secondary sort: Alphabetical by name if all else is equal.
              return Traits::GetAccountName(credential_a) <
                     Traits::GetAccountName(credential_b);
            });

  std::vector<T> actually_sorted_credentials;
  actually_sorted_credentials.reserve(deduplicated_credentials.size());
  for (size_t index : indices) {
    // Move from the input `deduplicated_credentials` vector which is a copy.
    actually_sorted_credentials.push_back(
        std::move(deduplicated_credentials[index]));
  }
  return actually_sorted_credentials;
}

template <typename T, typename Traits>
std::vector<T> ProcessCredentials(std::vector<T> credentials) {
  // 1. Group credentials by account name.
  //    Pointers in grouped_by_account point to elements in the `credentials`
  //    copy.
  std::map<std::u16string, std::vector<const T*>> grouped_by_account;
  for (const T& credential : credentials) {
    grouped_by_account[Traits::GetAccountName(credential)].push_back(
        &credential);
  }

  // 2. Deduplicate credentials.
  std::vector<T> deduplicated_credentials =
      DeduplicateCredentialsByAccount<T, Traits>(grouped_by_account);

  base::UmaHistogramBoolean(
      "WebAuthentication.MechanismSorter.DeduplicationHappened",
      credentials.size() > deduplicated_credentials.size());

  // 3. Sort the deduplicated credentials.
  return SortCredentials<T, Traits>(std::move(deduplicated_credentials));
}

}  // namespace webauthn::sorting

#endif  // CHROME_BROWSER_WEBAUTHN_CREDENTIAL_SORTER_H_
