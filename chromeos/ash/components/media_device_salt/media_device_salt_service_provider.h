// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_H_

#include "base/component_export.h"

class AccountId;

namespace media_device_salt {
class MediaDeviceSaltService;
}  // namespace media_device_salt

namespace ash {

// Provides the media_device_salt::MediaDeviceSaltService associated with a user
// to ChromeOS callers without forcing them to depend on
// //chrome/browser/media/webrtc's MediaDeviceSaltServiceFactory. The concrete
// implementation lives in //chrome (see
// //chrome/browser/ash/browser_delegate/keyed_service_provider/
// media_device_salt_service_provider_impl.h) and resolves the account to its
// BrowserContext before calling the factory.
class COMPONENT_EXPORT(MEDIA_DEVICE_SALT_SERVICE_PROVIDER)
    MediaDeviceSaltServiceProvider {
 public:
  MediaDeviceSaltServiceProvider();
  MediaDeviceSaltServiceProvider(const MediaDeviceSaltServiceProvider&) =
      delete;
  MediaDeviceSaltServiceProvider& operator=(
      const MediaDeviceSaltServiceProvider&) = delete;
  virtual ~MediaDeviceSaltServiceProvider();

  // Returns the process-wide singleton.
  static MediaDeviceSaltServiceProvider& Get();

  // Returns the MediaDeviceSaltService associated with `account_id`, or nullptr
  // if the embedder provides none. The returned pointer is owned by the
  // BrowserContext-keyed service infrastructure; callers must not delete it.
  virtual media_device_salt::MediaDeviceSaltService* Find(
      const AccountId& account_id) = 0;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_MEDIA_DEVICE_SALT_MEDIA_DEVICE_SALT_SERVICE_PROVIDER_H_
