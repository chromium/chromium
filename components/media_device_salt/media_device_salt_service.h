// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_
#define COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;

namespace media_device_salt {

class MediaDeviceIDSalt;

class MediaDeviceSaltService : public KeyedService {
 public:
  explicit MediaDeviceSaltService(PrefService* pref_service);
  ~MediaDeviceSaltService() override;

  MediaDeviceSaltService(const MediaDeviceSaltService&) = delete;
  MediaDeviceSaltService& operator=(const MediaDeviceSaltService&) = delete;

  void GetSalt(base::OnceCallback<void(const std::string&)> callback);
  void ResetSalt();

 private:
  const scoped_refptr<MediaDeviceIDSalt> media_device_id_salt_;
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace media_device_salt

#endif  // COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_H_
