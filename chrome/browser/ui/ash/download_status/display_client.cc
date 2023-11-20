// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_client.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"

namespace ash::download_status {

DisplayClient::DisplayClient(Profile* profile) : profile_(profile) {
  CHECK(profile_);
  profile_observation_.Observe(profile_);
}

DisplayClient::~DisplayClient() = default;

void DisplayClient::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

}  // namespace ash::download_status
