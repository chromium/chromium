// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems_provider.h"

#include "chrome/renderer/media/chrome_key_systems.h"
#include "third_party/widevine/cdm/buildflags.h"

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "third_party/widevine/cdm/widevine_cdm_common.h"  // nogncheck
#endif

ChromeKeySystemsProvider::ChromeKeySystemsProvider() = default;
ChromeKeySystemsProvider::~ChromeKeySystemsProvider() = default;

void ChromeKeySystemsProvider::GetSupportedKeySystems(
    media::GetSupportedKeySystemsCB cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!test_provider_.is_null()) {
    media::KeySystemPropertiesVector key_systems;
    test_provider_.Run(&key_systems);
    OnSupportedKeySystemsReady(std::move(cb), std::move(key_systems));
    return;
  }

  GetChromeKeySystems(
      base::BindOnce(&ChromeKeySystemsProvider::OnSupportedKeySystemsReady,
                     weak_factory_.GetWeakPtr(), std::move(cb)));
}

bool ChromeKeySystemsProvider::IsKeySystemsUpdateNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Always needs update if we have never updated, regardless the
  // |last_update_time_ticks_|'s initial value.
  if (!has_updated_) {
    DCHECK(is_update_needed_);
    return true;
  }

  if (!is_update_needed_)
    return false;

  // The update could be expensive. For example, it could involve an IPC to the
  // browser process. Use a minimum update interval to avoid unnecessarily
  // frequent update.
  static const int kMinUpdateIntervalInMilliseconds = 1000;
  if ((tick_clock_->NowTicks() - last_update_time_ticks_).InMilliseconds() <
      kMinUpdateIntervalInMilliseconds) {
    return false;
  }

  return true;
}

void ChromeKeySystemsProvider::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void ChromeKeySystemsProvider::SetProviderDelegateForTesting(
    KeySystemsProviderDelegate test_provider) {
  test_provider_ = std::move(test_provider);
}

void ChromeKeySystemsProvider::OnSupportedKeySystemsReady(
    media::GetSupportedKeySystemsCB cb,
    media::KeySystemPropertiesVector key_systems) {
  has_updated_ = true;
  last_update_time_ticks_ = tick_clock_->NowTicks();

// Check whether all potentially supported key systems are supported. If so,
// no need to update again.
#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  for (const auto& properties : key_systems) {
    if (properties->GetBaseKeySystemName() == kWidevineKeySystem) {
      is_update_needed_ = false;
    }
  }
#else
  is_update_needed_ = false;
#endif

  std::move(cb).Run(std::move(key_systems));
}
