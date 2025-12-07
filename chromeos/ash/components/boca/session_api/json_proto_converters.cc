// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/json_proto_converters.h"

#include "base/values.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"

namespace ash::boca {

::boca::UserIdentity ConvertUserIdentityJsonToProto(
    const base::Value::Dict* dict) {
  ::boca::UserIdentity user_identity;
  if (auto* email = dict->FindString(kEmail)) {
    user_identity.set_email(*email);
  }
  if (auto* gaia_id = dict->FindString(kGaiaId)) {
    user_identity.set_gaia_id(*gaia_id);
  }
  if (auto* full_name = dict->FindString(kFullName)) {
    user_identity.set_full_name(*full_name);
  }
  if (auto* photo_url = dict->FindString(kPhotoUrl)) {
    user_identity.set_photo_url(*photo_url);
  }
  return user_identity;
}

}  // namespace ash::boca
