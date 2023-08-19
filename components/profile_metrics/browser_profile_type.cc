// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/profile_metrics/browser_profile_type.h"

#include <memory>

#include "base/supports_user_data.h"

namespace profile_metrics {

namespace {

class ProfileTypeUserData : public base::SupportsUserData::Data {
 public:
  explicit ProfileTypeUserData(BrowserProfileType browser_context_type)
      : browser_context_type_(browser_context_type) {}

  ProfileTypeUserData(const ProfileTypeUserData&) = delete;
  ProfileTypeUserData& operator=(const ProfileTypeUserData&) = delete;

  static const void* const kKey;

  BrowserProfileType browser_context_type() const {
    return browser_context_type_;
  }

 private:
  const BrowserProfileType browser_context_type_;
};

const void* const ProfileTypeUserData::kKey = &ProfileTypeUserData::kKey;

}  // namespace

void SetBrowserProfileType(base::SupportsUserData* browser_context,
                           BrowserProfileType type) {
  browser_context->SetUserData(ProfileTypeUserData::kKey,
                               std::make_unique<ProfileTypeUserData>(type));
}

BrowserProfileType GetBrowserProfileType(
    const base::SupportsUserData* browser_context) {
  auto* profile_type_user_data = static_cast<ProfileTypeUserData*>(
      browser_context->GetUserData(ProfileTypeUserData::kKey));
  // We deliberately don't want to gracefully handle this data missing as all
  // `browser_context`s are supposed to be assigned a type as soon as they are
  // created.
  CHECK(profile_type_user_data);
  return profile_type_user_data->browser_context_type();
}

}  // namespace profile_metrics
