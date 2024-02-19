// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/system_signals/mac/mac_system_signals_service.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/system_signals/file_system_service.h"
#include "components/device_signals/core/system_signals/mock_file_system_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using device_signals::MockFileSystemService;
using testing::Return;

namespace system_signals {

class MacSystemSignalsServiceTest : public testing::Test {
 protected:
  MacSystemSignalsServiceTest() {
    auto file_system_service =
        std::make_unique<testing::StrictMock<MockFileSystemService>>();
    file_system_service_ = file_system_service.get();

    mojo::PendingReceiver<device_signals::mojom::SystemSignalsService>
        fake_receiver;

    // Have to use "new" since make_unique doesn't have access to friend private
    // constructor.
    mac_system_signals_service_ =
        std::unique_ptr<MacSystemSignalsService>(new MacSystemSignalsService(
            std::move(fake_receiver), std::move(file_system_service)));
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MacSystemSignalsService> mac_system_signals_service_;
  // Owned by mac_system_signals_service_.
  raw_ptr<MockFileSystemService> file_system_service_;
};

// Tests that GetFileSystemSignals forwards the signal collection to
// FileSystemService.
TEST_F(MacSystemSignalsServiceTest, GetFileSystemSignals) {
  device_signals::GetFileSystemInfoOptions options;
  options.file_path = base::FilePath::FromUTF8Unsafe("/some/file/path");

  std::vector<device_signals::GetFileSystemInfoOptions> requests;
  requests.push_back(std::move(options));

  device_signals::FileSystemItem returned_item;
  returned_item.file_path =
      base::FilePath::FromUTF8Unsafe("/some/other/file/path");
  returned_item.presence = device_signals::PresenceValue::kFound;

  std::vector<device_signals::FileSystemItem> response;
  response.push_back(std::move(returned_item));

  EXPECT_CALL(*file_system_service_, GetSignals(requests))
      .WillOnce(Return(response));

  base::test::TestFuture<const std::vector<device_signals::FileSystemItem>&>
      future;
  mac_system_signals_service_->GetFileSystemSignals(requests,
                                                    future.GetCallback());

  auto results = future.Get();
  EXPECT_EQ(results.size(), response.size());
  EXPECT_EQ(results[0], response[0]);
}

}  // namespace system_signals
