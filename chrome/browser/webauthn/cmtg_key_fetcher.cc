// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cmtg_key_fetcher.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "components/device_event_log/device_event_log.h"

CmtgKeyFetcher::CmtgKeyFetcher(webauthn::CmtgDeviceKeyProvider* provider,
                               const base::TickClock* tick_clock)
    : provider_(provider), timeout_(tick_clock) {}

CmtgKeyFetcher::~CmtgKeyFetcher() = default;

void CmtgKeyFetcher::Start() {
  if (!provider_) {
    is_ready_ = true;
    return;
  }

  FIDO_LOG(EVENT) << "Fetching CMTG device keys";
  timeout_.Start(FROM_HERE, GPMEnclaveController::kFetchDeviceKeysTimeout,
                 base::BindOnce(&CmtgKeyFetcher::OnTimeout,
                                weak_ptr_factory_.GetWeakPtr()));
  fetch_request_ = provider_->GetDeviceKeys(base::BindOnce(
      &CmtgKeyFetcher::OnKeysFetched, weak_ptr_factory_.GetWeakPtr()));
}

void CmtgKeyFetcher::WaitForKeys(Callback callback) {
  CHECK(!callback_);
  if (is_ready()) {
    std::move(callback).Run();
    return;
  }
  callback_ = std::move(callback);
  cmtg_blocking_timer_ = base::ElapsedTimer();
}

void CmtgKeyFetcher::OnKeysFetched(
    base::expected<std::vector<std::vector<uint8_t>>,
                   webauthn::CmtgDeviceKeyProvider::Error> keys) {
  fetch_request_.reset();
  timeout_.Stop();
  if (keys.has_value()) {
    FIDO_LOG(EVENT) << "Successfully fetched " << keys->size()
                    << " CMTG device keys";
    keys_ = std::move(*keys);
  } else {
    FIDO_LOG(ERROR) << "Failed to fetch CMTG device keys from Cryptauth: error "
                    << static_cast<int>(keys.error());
  }
  is_ready_ = true;
  RecordMetricsAndMaybeRunCallback();
}

void CmtgKeyFetcher::OnTimeout() {
  FIDO_LOG(EVENT) << "CMTG device key fetch timed out";
  fetch_request_.reset();
  is_ready_ = true;
  RecordMetricsAndMaybeRunCallback();
}

void CmtgKeyFetcher::RecordMetricsAndMaybeRunCallback() {
  if (cmtg_blocking_timer_) {
    base::UmaHistogramTimes("WebAuthentication.Cmtg.BlockedDelay",
                            cmtg_blocking_timer_->Elapsed());
    cmtg_blocking_timer_.reset();
  }
  if (callback_) {
    std::move(callback_).Run();
  }
}
