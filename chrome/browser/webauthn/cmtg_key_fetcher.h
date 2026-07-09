// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_CMTG_KEY_FETCHER_H_
#define CHROME_BROWSER_WEBAUTHN_CMTG_KEY_FETCHER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"

namespace base {
class TickClock;
}

// CmtgKeyFetcher handles fetching CMTG (Credential Manager Trust Group) device
// keys from Cryptauth, including handling timeouts and recording blocking
// latency metrics if the client transaction is delayed waiting for the keys.
class CmtgKeyFetcher {
 public:
  using Callback = base::OnceClosure;

  CmtgKeyFetcher(webauthn::CmtgDeviceKeyProvider* provider,
                 const base::TickClock* tick_clock);
  ~CmtgKeyFetcher();

  // Starts the fetch process.
  void Start();

  // Returns true if the keys are ready (or fetch failed/timed out).
  bool is_ready() const { return is_ready_; }

  // Returns true if there is a pending callback waiting for keys.
  bool is_waiting_for_keys() const { return !callback_.is_null(); }

  // Returns the fetched keys, or nullopt if the fetch failed or timed out.
  // Should only be called if is_ready() is true.
  std::optional<std::vector<std::vector<uint8_t>>> keys() { return keys_; }

  // Starts the blocking latency timer and registers a callback to be invoked
  // when keys are ready.
  void WaitForKeys(Callback callback);

 private:
  void OnKeysFetched(
      base::expected<std::vector<std::vector<uint8_t>>,
                     webauthn::CmtgDeviceKeyProvider::Error> keys);
  void OnTimeout();
  void RecordMetricsAndMaybeRunCallback();

  const raw_ptr<webauthn::CmtgDeviceKeyProvider> provider_;
  Callback callback_;
  std::unique_ptr<webauthn::CmtgDeviceKeyProvider::Request> fetch_request_;
  base::OneShotTimer timeout_;
  std::optional<base::ElapsedTimer> cmtg_blocking_timer_;
  std::optional<std::vector<std::vector<uint8_t>>> keys_;
  bool is_ready_ = false;
  base::WeakPtrFactory<CmtgKeyFetcher> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_WEBAUTHN_CMTG_KEY_FETCHER_H_
