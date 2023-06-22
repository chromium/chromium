// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_salt_service.h"

#include "components/media_device_salt/media_device_id_salt.h"

namespace media_device_salt {

MediaDeviceSaltService::MediaDeviceSaltService(PrefService* pref_service)
    : media_device_id_salt_(
          base::MakeRefCounted<MediaDeviceIDSalt>(pref_service)),
      pref_service_(pref_service) {}

MediaDeviceSaltService::~MediaDeviceSaltService() = default;

void MediaDeviceSaltService::GetSalt(
    base::OnceCallback<void(const std::string&)> callback) {
  std::move(callback).Run(media_device_id_salt_->GetSalt());
}

void MediaDeviceSaltService::ResetSalt() {
  MediaDeviceIDSalt::Reset(pref_service_);
}

}  // namespace media_device_salt
