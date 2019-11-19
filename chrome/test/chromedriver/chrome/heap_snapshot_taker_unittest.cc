// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"

#include <stddef.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char* const chunks[] = {"{\"a\": 1,", "\"b\": 2}"};

std::unique_ptr<base::Value> GetSnapshotAsValue() {
  std::string str_snapshot = "{\"a\": 1,\"b\": 2}";
  return std::make_unique<base::Value>(std::move(str_snapshot));;
}

class DummyDevToolsClient : public StubDevToolsClient {
 public:
  DummyDevToolsClient(const std::string& method, bool error_after_events)
      : method_(method),
        error_after_events_(error_after_events),
        uid_(1),
        disabled_(false) {}
  ~DummyDevToolsClient() override {}

  bool IsDisabled() { return disabled_; }

  Status SendAddHeapSnapshotChunkEvent() {
    base::DictionaryValue event_params;
    event_params.SetInteger("uid", uid_);
    for (size_t i = 0; i < base::size(chunks); ++i) {
      event_params.SetString("chunk", chunks[i]);
      Status status = listeners_.front()->OnEvent(
          this, "HeapProfiler.addHeapSnapshotChunk", event_params);
      if (status.IsError())
        return status;
    }
    return Status(kOk);
  }

  // Overridden from DevToolsClient:
  Status SendCommand(const std::string& method,
                     const base::DictionaryValue& params) override {
    if (!disabled_)
      disabled_ = method == "Debugger.disable";
    if (method == method_ && !error_after_events_)
      return Status(kUnknownError);

    if (method == "HeapProfiler.takeHeapSnapshot") {
      Status status = SendAddHeapSnapshotChunkEvent();
      if (status.IsError())
        return status;
    }

    if (method == method_ && error_after_events_)
      return Status(kUnknownError);
    return StubDevToolsClient::SendCommand(method, params);
  }

 protected:
  std::string method_;  // Throw error on command with this method.
  bool error_after_events_;
  int uid_;
  bool disabled_;  // True if Debugger.disable was issued.
};

}  // namespace

TEST(HeapSnapshotTaker, SuccessfulCase) {
  DummyDevToolsClient client("", false);
  HeapSnapshotTaker taker(&client);
  std::unique_ptr<base::Value> snapshot;
  Status status = taker.TakeSnapshot(&snapshot);
  ASSERT_EQ(kOk, status.code());
  ASSERT_TRUE(GetSnapshotAsValue()->Equals(snapshot.get()));
  ASSERT_TRUE(client.IsDisabled());
}

TEST(HeapSnapshotTaker, FailIfErrorOnDebuggerEnable) {
  DummyDevToolsClient client("Debugger.enable", false);
  HeapSnapshotTaker taker(&client);
  std::unique_ptr<base::Value> snapshot;
  Status status = taker.TakeSnapshot(&snapshot);
  ASSERT_TRUE(status.IsError());
  ASSERT_FALSE(snapshot.get());
  ASSERT_TRUE(client.IsDisabled());
}

TEST(HeapSnapshotTaker, FailIfErrorOnCollectGarbage) {
  DummyDevToolsClient client("HeapProfiler.collectGarbage", false);
  HeapSnapshotTaker taker(&client);
  std::unique_ptr<base::Value> snapshot;
  Status status = taker.TakeSnapshot(&snapshot);
  ASSERT_TRUE(status.IsError());
  ASSERT_FALSE(snapshot.get());
  ASSERT_TRUE(client.IsDisabled());
}

TEST(HeapSnapshotTaker, ErrorBeforeWhenReceivingSnapshot) {
  DummyDevToolsClient client("HeapProfiler.takeHeapSnapshot", false);
  HeapSnapshotTaker taker(&client);
  std::unique_ptr<base::Value> snapshot;
  Status status = taker.TakeSnapshot(&snapshot);
  ASSERT_TRUE(status.IsError());
  ASSERT_FALSE(snapshot.get());
  ASSERT_TRUE(client.IsDisabled());
}

