// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/lacros/system_idle_cache.h"

#include "chromeos/lacros/lacros_service.h"

namespace chromeos {

SystemIdleCache::SystemIdleCache() : is_fallback_(true) {}

SystemIdleCache::SystemIdleCache(const crosapi::mojom::IdleInfo& info)
    : is_fallback_(false), info_(info.Clone()) {}

SystemIdleCache::~SystemIdleCache() = default;

void SystemIdleCache::Start() {
  DCHECK(!is_fallback_);
  auto* lacros_service = chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::IdleService>());
  lacros_service->GetRemote<crosapi::mojom::IdleService>()->AddIdleInfoObserver(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

base::TimeDelta SystemIdleCache::auto_lock_delay() const {
  return is_fallback_ ? base::TimeDelta() : info_->auto_lock_delay;
}

base::TimeTicks SystemIdleCache::last_activity_time() const {
  return is_fallback_ ? base::TimeTicks::Now() : info_->last_activity_time;
}

bool SystemIdleCache::is_locked() const {
  return is_fallback_ ? false : info_->is_locked;
}

void SystemIdleCache::OnIdleInfoChanged(crosapi::mojom::IdleInfoPtr info) {
  info_ = std::move(info);
}

}  // namespace chromeos
