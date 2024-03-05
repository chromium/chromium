// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/le_scan_manager_impl.h"

#include <algorithm>
#include <deque>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/public/cast_media_shlib.h"

#define RUN_ON_IO_THREAD(method, ...)                       \
  io_task_runner_->PostTask(                                \
      FROM_HERE, base::BindOnce(&LeScanManagerImpl::method, \
                                weak_factory_.GetWeakPtr(), ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

#define EXEC_CB_AND_RET(cb, ret, ...)        \
  do {                                       \
    if (cb) {                                \
      std::move(cb).Run(ret, ##__VA_ARGS__); \
    }                                        \
    return;                                  \
  } while (0)

namespace chromecast {
namespace bluetooth {

namespace {

const int kMaxMessagesInQueue = 5;

}  // namespace

// static
constexpr int LeScanManagerImpl::kMaxScanResultEntries;

// static
std::unique_ptr<LeScanManager> LeScanManager::Create(
    BluetoothManagerPlatform* bluetooth_manager,
    bluetooth_v2_shlib::LeScannerImpl* le_scanner) {
  return std::make_unique<LeScanManagerImpl>(le_scanner);
}

class LeScanManagerImpl::ScanHandleImpl : public LeScanManager::ScanHandle {
 public:
  explicit ScanHandleImpl(LeScanManagerImpl* manager, int32_t id)
      : on_destroyed_(BindToCurrentSequence(
            base::BindOnce(&LeScanManagerImpl::NotifyScanHandleDestroyed,
                           manager->weak_factory_.GetWeakPtr(),
                           id))) {}
  ~ScanHandleImpl() override { std::move(on_destroyed_).Run(); }

 private:
  base::OnceClosure on_destroyed_;
};

LeScanManagerImpl::LeScanManagerImpl(
    bluetooth_v2_shlib::LeScannerImpl* le_scanner)
    : le_scanner_(le_scanner),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      weak_factory_(this) {}

LeScanManagerImpl::~LeScanManagerImpl() = default;

void LeScanManagerImpl::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  io_task_runner_ = std::move(io_task_runner);
  InitializeOnIoThread();
}

void LeScanManagerImpl::Finalize() {}

void LeScanManagerImpl::InitializeOnIoThread() {
  MAKE_SURE_IO_THREAD(InitializeOnIoThread);
  le_scanner_->SetDelegate(this);
}

void LeScanManagerImpl::AddObserver(Observer* observer) {
  observers_->AddObserver(observer);
}

void LeScanManagerImpl::RemoveObserver(Observer* observer) {
  observers_->RemoveObserver(observer);
}

void LeScanManagerImpl::RequestScan(RequestScanCallback cb) {
  MAKE_SURE_IO_THREAD(RequestScan, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << __func__;

  if (scan_handle_ids_.empty()) {
    if (!le_scanner_->StartScan()) {
      LOG(ERROR) << "Failed to enable scanning";
      std::move(cb).Run(nullptr);
      return;
    }
    LOG(INFO) << "Enabling scan";
    observers_->Notify(FROM_HERE, &Observer::OnScanEnableChanged, true);
  }

  int32_t id = next_scan_handle_id_++;
  auto handle = std::make_unique<ScanHandleImpl>(this, id);
  scan_handle_ids_.insert(id);

  std::move(cb).Run(std::move(handle));
}

void LeScanManagerImpl::GetScanResults(GetScanResultsCallback cb,
                                       std::optional<ScanFilter> scan_filter) {
  MAKE_SURE_IO_THREAD(GetScanResults, BindToCurrentSequence(std::move(cb)),
                      std::move(scan_filter));
  std::move(cb).Run(GetScanResultsInternal(std::move(scan_filter)));
}

void LeScanManagerImpl::ClearScanResults() {
  MAKE_SURE_IO_THREAD(ClearScanResults);
  addr_to_scan_results_.clear();
}

void LeScanManagerImpl::PauseScan() {
  MAKE_SURE_IO_THREAD(PauseScan);
  if (scan_handle_ids_.empty()) {
    LOG(ERROR) << "Can't pause scan, no scan handle";
    return;
  }

  if (!le_scanner_->StopScan()) {
    LOG(ERROR) << "Failed to pause scanning";
  }
}

void LeScanManagerImpl::ResumeScan() {
  MAKE_SURE_IO_THREAD(ResumeScan);
  if (scan_handle_ids_.empty()) {
    LOG(ERROR) << "Can't restart scan, no scan handle";
    return;
  }

  if (!le_scanner_->StartScan()) {
    LOG(ERROR) << "Failed to restart scanning";
  }
}

void LeScanManagerImpl::SetScanParameters(int scan_interval_ms,
                                          int scan_window_ms) {
  MAKE_SURE_IO_THREAD(SetScanParameters, scan_interval_ms, scan_window_ms);
  if (scan_handle_ids_.empty()) {
    LOG(ERROR) << "Can't set scan parameters, no scan handle";
    return;
  }

  // We could only set scan parameters when scan is paused.
  if (!le_scanner_->StopScan()) {
    LOG(ERROR) << "Failed to pause scanning before setting scan parameters";
    return;
  }

  if (!le_scanner_->SetScanParameters(scan_interval_ms, scan_window_ms)) {
    LOG(ERROR) << "Failed to set scan parameters";
    return;
  }

  if (!le_scanner_->StartScan()) {
    LOG(ERROR) << "Failed to restart scanning after setting scan parameters";
    return;
  }

  LOG(INFO) << __func__ << " scan_interval: " << scan_interval_ms
            << "ms scan_window: " << scan_window_ms << "ms";
}

void LeScanManagerImpl::OnScanResult(
    const bluetooth_v2_shlib::LeScanner::ScanResult& scan_result_shlib) {
  LeScanResult scan_result;
  if (!scan_result.SetAdvData(scan_result_shlib.adv_data)) {
    // Error logged.
    return;
  }
  scan_result.addr = scan_result_shlib.addr;
  scan_result.rssi = scan_result_shlib.rssi;

  auto& previous_scan_results = addr_to_scan_results_[scan_result.addr];
  if (previous_scan_results.size() > 0) {
    // Remove results with the same data as the current result to avoid
    // duplicate messages in the queue
    previous_scan_results.remove_if(
        [&scan_result](const auto& previous_result) {
          return previous_result.adv_data == scan_result.adv_data;
        });

    // Remove scan_result.addr to avoid duplicate addresses in
    // recent_scan_result_addr_list_.
    std::erase(scan_result_addr_list_, scan_result.addr);
  }

  previous_scan_results.push_front(scan_result);
  if (previous_scan_results.size() > kMaxMessagesInQueue) {
    previous_scan_results.pop_back();
  }

  // Update recent_scan_result_addr_list_.
  scan_result_addr_list_.push_front(scan_result.addr);
  while (scan_result_addr_list_.size() > kMaxScanResultEntries) {
    // Remove least recently used address in recent_scan_result_addr_list_.
    auto least_recently_used_addr = scan_result_addr_list_.back();
    scan_result_addr_list_.pop_back();
    addr_to_scan_results_.erase(least_recently_used_addr);
  }

  // Update observers.
  observers_->Notify(FROM_HERE, &Observer::OnNewScanResult, scan_result);
}

// Returns a list of all scan results. The results are sorted by RSSI.
std::vector<LeScanResult> LeScanManagerImpl::GetScanResultsInternal(
    std::optional<ScanFilter> scan_filter) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::vector<LeScanResult> results;
  for (const auto& pair : addr_to_scan_results_) {
    for (const auto& scan_result : pair.second) {
      if (!scan_filter || scan_filter->Matches(scan_result)) {
        results.push_back(scan_result);
      }
    }
  }

  std::sort(results.begin(), results.end(),
            [](const LeScanResult& d1, const LeScanResult& d2) {
              return d1.rssi > d2.rssi;
            });

  return results;
}

void LeScanManagerImpl::NotifyScanHandleDestroyed(int32_t id) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  size_t num_removed = scan_handle_ids_.erase(id);
  DCHECK_EQ(num_removed, 1u);
  if (scan_handle_ids_.empty()) {
    if (!le_scanner_->StopScan()) {
      LOG(ERROR) << "Failed to disable scanning";
    } else {
      LOG(INFO) << "Disabling scan";
      observers_->Notify(FROM_HERE, &Observer::OnScanEnableChanged, false);
    }
  }
}

}  // namespace bluetooth
}  // namespace chromecast
