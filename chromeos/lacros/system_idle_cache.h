// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LACROS_SYSTEM_IDLE_CACHE_H_
#define CHROMEOS_LACROS_SYSTEM_IDLE_CACHE_H_

#include "base/time/time.h"
#include "chromeos/crosapi/mojom/idle_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

// Provider of synchronous API to read idle info, used to compute idle state of
// the system. Depending on IsIdleServiceAvailable(), this operates under one of
// the following modes:
// * false => Fallback mode: The getters produce fallback values to placate the
//   caller.
// * true => Streaming mode: The instance connects to ash-chrome, listens to
//   system idle info stream, and caches the info for later synchronous reads
//   using getters.
class COMPONENT_EXPORT(CHROMEOS_LACROS) SystemIdleCache
    : public crosapi::mojom::IdleInfoObserver {
 public:
  // TODO(huangs): Remove when ash-chrome reaches M91.
  // Instantiates under Fallback mode.
  SystemIdleCache();

  // Instantiates under Streaming mode, caching |info| iniitally.
  explicit SystemIdleCache(const crosapi::mojom::IdleInfo& info);

  SystemIdleCache(const SystemIdleCache&) = delete;
  SystemIdleCache& operator=(const SystemIdleCache&) = delete;
  ~SystemIdleCache() override;

  // Streaming mode only: Start observing idle info changes in ash-chrome.
  // This is a post-construction step to decouple from LacrosService.
  void Start();

  // Getters: These can be used even before Start() gets called.

  // Idle time for auto lock to kick in, with 0 meaning auto lock is disabled.
  // Fallback: Empty time (becomes 0) to pretend that auto lock is disabled.
  base::TimeDelta auto_lock_delay() const;

  // Most recent time that user is active. Fallback: Current time to pretend
  // that the user is always active.
  base::TimeTicks last_activity_time() const;

  // Whether the system is locked. Fallback: false to pretend that the user is
  // always logged in.
  bool is_locked() const;

 private:
  // crosapi::mojom::IdleInfoObserver:
  void OnIdleInfoChanged(crosapi::mojom::IdleInfoPtr info) override;

  // True for Fallback mode, false for Streaming mode.
  const bool is_fallback_;

  // Cached idle info.
  crosapi::mojom::IdleInfoPtr info_;

  // Receives mojo messages from ash-chromem (under Streaming mode).
  mojo::Receiver<crosapi::mojom::IdleInfoObserver> receiver_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_LACROS_SYSTEM_IDLE_CACHE_H_
