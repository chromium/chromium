// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTINGS_H_
#define COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTINGS_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/account_settings/account_settings_features.h"

namespace account_settings {

// Metadata for account settings. This struct only contains the definition of a
// setting (name, type and optional feature flag). The actual value of the
// setting must be queried from `AccountSettingService`.
struct AccountSetting {
  const char* const name;
  base::Value::Type type;
  // Optional feature flag to gate the setting.
  raw_ptr<const base::Feature> feature = nullptr;
};

// Available settings

// Boolean setting on whether the user agreed to share public pass data from
// Wallet to other Google services, including Chrome, in their Google account
// settings.
inline constexpr AccountSetting kWalletPrivacyContextualSurfacing{
    "WALLET_PRIVACY_CONTEXTUAL_SURFACING", base::Value::Type::BOOLEAN};

inline constexpr AccountSetting kAccountSettingContext{
    "GEMINI_PERSONAL_CONTEXT", base::Value::Type::BOOLEAN,
    &features::kAccountSettingContext};

inline constexpr AccountSetting kAccountSettingContextWorkspace{
    "PERSONAL_CONTEXT_GEMINI_USING_WORKSPACE", base::Value::Type::BOOLEAN,
    &features::kAccountSettingContext};

inline constexpr AccountSetting kAccountSettingContextPhotos{
    "PERSONAL_CONTEXT_GEMINI_USING_PHOTOS", base::Value::Type::BOOLEAN,
    &features::kAccountSettingContext};

inline constexpr AccountSetting kAccountSettingContextSearch{
    "GEMINI_PERSONAL_CONTEXT_SEARCH", base::Value::Type::BOOLEAN,
    &features::kAccountSettingContext};

inline constexpr AccountSetting kAccountSettingContextYoutube{
    "GEMINI_PERSONAL_CONTEXT_YOUTUBE", base::Value::Type::BOOLEAN,
    &features::kAccountSettingContext};

}  // namespace account_settings

#endif  // COMPONENTS_ACCOUNT_SETTINGS_ACCOUNT_SETTINGS_H_
