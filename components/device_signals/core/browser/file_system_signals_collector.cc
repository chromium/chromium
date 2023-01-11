// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/file_system_signals_collector.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

namespace device_signals {

FileSystemSignalsCollector::FileSystemSignalsCollector(
    SystemSignalsServiceHost* system_service_host)
    : BaseSignalsCollector({
          {SignalName::kFileSystemInfo,
           base::BindRepeating(
               &FileSystemSignalsCollector::GetFileSystemInfoSignal,
               base::Unretained(this))},
      }),
      system_service_host_(system_service_host) {
  DCHECK(system_service_host_);
}

FileSystemSignalsCollector::~FileSystemSignalsCollector() = default;

void FileSystemSignalsCollector::GetFileSystemInfoSignal(
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  if (request.file_system_signal_parameters.empty()) {
    FileSystemInfoResponse signal_response;
    signal_response.collection_error =
        SignalCollectionError::kMissingParameters;
    response.file_system_info_response = std::move(signal_response);
    std::move(done_closure).Run();
    return;
  }

  auto* system_signals_service = system_service_host_->GetService();
  if (!system_signals_service) {
    FileSystemInfoResponse signal_response;
    signal_response.collection_error =
        SignalCollectionError::kMissingSystemService;
    response.file_system_info_response = std::move(signal_response);
    std::move(done_closure).Run();
    return;
  }

  system_signals_service->GetFileSystemSignals(
      request.file_system_signal_parameters,
      base::BindOnce(&FileSystemSignalsCollector::OnFileSystemSignalCollected,
                     weak_factory_.GetWeakPtr(), std::ref(response),
                     std::move(done_closure)));
}

void FileSystemSignalsCollector::OnFileSystemSignalCollected(
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    const std::vector<FileSystemItem>& file_system_items) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  FileSystemInfoResponse signal_response;
  signal_response.file_system_items = std::move(file_system_items);
  response.file_system_info_response = std::move(signal_response);
  std::move(done_closure).Run();
}

}  // namespace device_signals
