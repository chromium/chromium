// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_device_salt/media_device_id_salt.h"

#include "base/base64.h"
#include "base/rand_util.h"
#include "base/system/system_monitor.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/media_device_id.h"

using content::BrowserThread;

namespace media_device_salt {

namespace prefs {
const char kMediaDeviceIdSalt[] = "media.device_id_salt";
}

MediaDeviceIDSalt::MediaDeviceIDSalt(PrefService* pref_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  media_device_id_salt_.Init(prefs::kMediaDeviceIdSalt, pref_service);
  if (media_device_id_salt_.GetValue().empty()) {
    media_device_id_salt_.SetValue(base::UnguessableToken::Create().ToString());
  }
}

MediaDeviceIDSalt::~MediaDeviceIDSalt() = default;

std::string MediaDeviceIDSalt::GetSalt() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return media_device_id_salt_.GetValue();
}

void MediaDeviceIDSalt::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(prefs::kMediaDeviceIdSalt, std::string());
}

void MediaDeviceIDSalt::Reset(PrefService* pref_service) {
  pref_service->SetString(prefs::kMediaDeviceIdSalt,
                          base::UnguessableToken::Create().ToString());
}

}  // namespace media_device_salt
