// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"

#include <stddef.h>

#include <utility>

#include "base/json/json_reader.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

HeapSnapshotTaker::HeapSnapshotTaker(DevToolsClient* client)
    : client_(client) {
  client_->AddListener(this);
}

HeapSnapshotTaker::~HeapSnapshotTaker() {}

Status HeapSnapshotTaker::TakeSnapshot(std::unique_ptr<base::Value>* snapshot) {
  Status status1 = TakeSnapshotInternal();
  base::Value::Dict params;
  Status status2 = client_->SendCommand("Debugger.disable", params);

  Status status3(kOk);
  if (status1.IsOk() && status2.IsOk()) {
    *snapshot = std::make_unique<base::Value>(std::move(snapshot_));
  }
  snapshot_.clear();
  if (status1.IsError()) {
    return status1;
  } else if (status2.IsError()) {
    return status2;
  } else {
    return status3;
  }
}

Status HeapSnapshotTaker::TakeSnapshotInternal() {
  base::Value::Dict params;
  const char* const kMethods[] = {
      "Debugger.enable",
      "HeapProfiler.collectGarbage",
      "HeapProfiler.takeHeapSnapshot"
  };
  for (size_t i = 0; i < std::size(kMethods); ++i) {
    Status status = client_->SendCommand(kMethods[i], params);
    if (status.IsError())
      return status;
  }

  return Status(kOk);
}

bool HeapSnapshotTaker::ListensToConnections() const {
  return false;
}

Status HeapSnapshotTaker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::Value::Dict& params) {
  if (method == "HeapProfiler.addHeapSnapshotChunk") {
    const std::string* chunk = params.FindString("chunk");
    if (!chunk) {
      return Status(kUnknownError,
                    "HeapProfiler.addHeapSnapshotChunk has no 'chunk'");
    }
    snapshot_.append(*chunk);
  }
  return Status(kOk);
}
