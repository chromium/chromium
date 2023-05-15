// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_FILE_SYSTEM_SIGNALS_COLLECTOR_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_FILE_SYSTEM_SIGNALS_COLLECTOR_H_

#include <map>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/device_signals/core/browser/base_signals_collector.h"

namespace device_signals {

struct FileSystemItem;
class SystemSignalsServiceHost;

// Collector in charge of collecting device signals that live in the file
// system.
class FileSystemSignalsCollector : public BaseSignalsCollector {
 public:
  explicit FileSystemSignalsCollector(
      SystemSignalsServiceHost* system_service_host);

  ~FileSystemSignalsCollector() override;

  FileSystemSignalsCollector(const FileSystemSignalsCollector&) = delete;
  FileSystemSignalsCollector& operator=(const FileSystemSignalsCollector&) =
      delete;

 private:
  // Collection function for the File System Info signal. `request` must contain
  // the required parameters for this signal. `response` will be passed along
  // and the signal values will be set on it when available. `done_closure` will
  // be invoked when signal collection is complete.
  void GetFileSystemInfoSignal(const SignalsAggregationRequest& request,
                               SignalsAggregationResponse& response,
                               base::OnceClosure done_closure);

  // Invoked when the SystemSignalsService returns the collected File System
  // items' signals as `file_system_items`. Will update `response` with the
  // signal collection outcome, and invoke `done_closure` to asynchronously
  // notify the caller of the completion of this request.
  void OnFileSystemSignalCollected(
      SignalsAggregationResponse& response,
      base::OnceClosure done_closure,
      const std::vector<FileSystemItem>& file_system_items);

  // Instance used to retrieve a pointer to a SystemSignalsService instance.
  raw_ptr<SystemSignalsServiceHost> system_service_host_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<FileSystemSignalsCollector> weak_factory_{this};
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_FILE_SYSTEM_SIGNALS_COLLECTOR_H_
