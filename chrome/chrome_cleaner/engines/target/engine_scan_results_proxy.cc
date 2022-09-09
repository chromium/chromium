// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"

namespace chrome_cleaner {

EngineScanResultsProxy::EngineScanResultsProxy(
    mojo::PendingAssociatedRemote<mojom::EngineScanResults> scan_results,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
  scan_results_.Bind(std::move(scan_results));
}

void EngineScanResultsProxy::UnbindScanResults() {
  scan_results_.reset();
}

void EngineScanResultsProxy::FoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&EngineScanResultsProxy::OnFoundUwS, this, pup_id, pup));
}

void EngineScanResultsProxy::ScanDone(uint32_t result) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&EngineScanResultsProxy::OnDone, this, result));
}

EngineScanResultsProxy::EngineScanResultsProxy() = default;

EngineScanResultsProxy::~EngineScanResultsProxy() = default;

// Invokes scan_results_->FoundUwS from the IPC thread.
void EngineScanResultsProxy::OnFoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  if (!scan_results_.is_bound()) {
    LOG(ERROR) << "Found UwS reported after the engine was shut down";
    return;
  }
  scan_results_->FoundUwS(pup_id, pup);
}

// Invokes scan_results_->Done from the IPC thread.
void EngineScanResultsProxy::OnDone(uint32_t result) {
  if (!scan_results_.is_bound()) {
    LOG(ERROR) << "Scan result reported after the engine was shut down";
    return;
  }
  scan_results_->Done(result);
}

}  // namespace chrome_cleaner
