// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_MOCK_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_MOCK_H_

#include <vector>

#include "chrome/chrome_cleaner/engines/broker/engine_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_cleaner {

class MockEngineClient : public EngineClient {
 public:
  // NullTaskRunner doesn't actually run any tasks. This works because every
  // method of EngineClient that uses the task runner is overridden except the
  // destructor. Objects that are cleaned up with PostTask in the EngineClient
  // destructor will leak but this isn't important enough to write extra
  // cleanup code for tests.
  MockEngineClient();

  // To test |PostBindEngineCommandsRemote|, mock the method
  // |MockedPostBindEngineCommandsRemote|. This is needed because |pipe| is
  // move-only and gmock generates a call to the copy constructor.
  void PostBindEngineCommandsRemote(
      mojo::ScopedMessagePipeHandle pipe) override {
    MockedPostBindEngineCommandsRemote(&pipe);
  }
  MOCK_METHOD1(MockedPostBindEngineCommandsRemote,
               void(mojo::ScopedMessagePipeHandle* pipe));

  MOCK_CONST_METHOD0(GetEnabledUwS, std::vector<UwSId>());

  MOCK_METHOD0(Initialize, uint32_t());

  // To test |StartScan|, mock the method |MockedStartScan|. This is needed
  // because |done_callback| is move-only and gmock generates a call to the copy
  // constructor.
  MOCK_METHOD5(
      MockedStartScan,
      uint32_t(const std::vector<UwSId>& enabled_uws,
               const std::vector<UwS::TraceLocation>& enabled_locations,
               bool include_details,
               FoundUwSCallback found_callback,
               DoneCallback* done_callback));
  uint32_t StartScan(const std::vector<UwSId>& enabled_uws,
                     const std::vector<UwS::TraceLocation>& enabled_locations,
                     bool include_details,
                     FoundUwSCallback found_callback,
                     DoneCallback done_callback) override {
    return MockedStartScan(enabled_uws, enabled_locations, include_details,
                           found_callback, &done_callback);
  }

  // To test |StartCleanup|, mock the method |MockedStartCleanup|. This is
  // needed because |done_callback| is move-only and gmock generates a call to
  // the copy constructor.
  MOCK_METHOD2(MockedStartCleanup,
               uint32_t(const std::vector<UwSId>& enabled_uws,
                        DoneCallback* done_callback));
  uint32_t StartCleanup(const std::vector<UwSId>& enabled_uws,
                        DoneCallback done_callback) {
    return MockedStartCleanup(enabled_uws, &done_callback);
  }

  MOCK_METHOD0(Finalize, uint32_t());
  MOCK_CONST_METHOD0(needs_reboot, bool());

 protected:
  ~MockEngineClient() override;
};

// Override of StrictMock to make the destructor protected, so that it can be
// used with a RefCounted base class.
class StrictMockEngineClient : public ::testing::StrictMock<MockEngineClient> {
 public:
  StrictMockEngineClient();

 protected:
  ~StrictMockEngineClient() override;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_MOCK_H_
