// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/browser/identifiers/profile_id_service.h"

#include <utility>

#include "base/base64url.h"
#include "base/check.h"
#include "base/hash/sha1.h"
#include "components/enterprise/browser/identifiers/identifiers_prefs.h"
#include "components/enterprise/browser/identifiers/profile_id_delegate.h"
#include "components/prefs/pref_service.h"

namespace enterprise {

ProfileIdService::ProfileIdService(std::unique_ptr<ProfileIdDelegate> delegate,
                                   PrefService* profile_prefs)
    : delegate_(std::move(delegate)), profile_prefs_(profile_prefs) {
  DCHECK(delegate_);
  DCHECK(profile_prefs_);
}

ProfileIdService::~ProfileIdService() = default;

absl::optional<std::string> ProfileIdService::GetProfileId() {
  std::string profile_guid = profile_prefs_->GetString(kProfileGUIDPref);
  if (profile_guid.empty())
    return absl::nullopt;

  auto device_id = delegate_->GetDeviceId();
  if (device_id.empty())
    return absl::nullopt;

  std::string encoded_string;
  base::Base64UrlEncode(base::SHA1HashString(profile_guid + device_id),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_string);
  return encoded_string;
}

}  // namespace enterprise
