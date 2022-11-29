// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_HEAP_SNAPSHOT_TAKER_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_HEAP_SNAPSHOT_TAKER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"

class DevToolsClient;
class Status;

// Take the heap snapshot.
class HeapSnapshotTaker : public DevToolsEventListener {
 public:
  explicit HeapSnapshotTaker(DevToolsClient* client);

  HeapSnapshotTaker(const HeapSnapshotTaker&) = delete;
  HeapSnapshotTaker& operator=(const HeapSnapshotTaker&) = delete;

  ~HeapSnapshotTaker() override;

  Status TakeSnapshot(std::unique_ptr<base::Value>* snapshot);

  // Overridden from DevToolsEventListener:
  bool ListensToConnections() const override;
  Status OnEvent(DevToolsClient* client,
                 const std::string& method,
                 const base::Value::Dict& params) override;

 private:
  Status TakeSnapshotInternal();

  raw_ptr<DevToolsClient> client_;
  std::string snapshot_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_HEAP_SNAPSHOT_TAKER_H_
