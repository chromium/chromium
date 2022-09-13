// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/file_system_signals_collector.h"

#include <array>
#include <utility>

#include "base/files/file_path.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/device_signals/core/browser/mock_system_signals_service_host.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ContainerEq;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;

namespace device_signals {

namespace {

SignalsAggregationRequest CreateRequest(SignalName signal_name,
                                        bool with_file_parameter = true) {
  SignalsAggregationRequest request;
  request.signal_names.emplace(signal_name);

  if (with_file_parameter) {
    GetFileSystemInfoOptions options1;
    options1.file_path = base::FilePath::FromUTF8Unsafe("some file path");
    options1.compute_sha256 = true;
    options1.compute_executable_metadata = true;

    GetFileSystemInfoOptions options2;
    options2.file_path = base::FilePath::FromUTF8Unsafe("some file path");
    options2.compute_sha256 = true;
    options2.compute_executable_metadata = true;

    request.file_system_signal_parameters.push_back(options1);
    request.file_system_signal_parameters.push_back(options2);
  }

  return request;
}
}  // namespace

using GetFileSystemSignalsCallback =
    MockSystemSignalsService::GetFileSystemSignalsCallback;

class FileSystemSignalsCollectorTest : public testing::Test {
 protected:
  FileSystemSignalsCollectorTest() : signal_collector_(&service_host_) {
    ON_CALL(service_host_, GetService()).WillByDefault(Return(&service_));
  }

  base::test::TaskEnvironment task_environment_;

  StrictMock<MockSystemSignalsServiceHost> service_host_;
  StrictMock<MockSystemSignalsService> service_;
  FileSystemSignalsCollector signal_collector_;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
TEST_F(FileSystemSignalsCollectorTest, SupportedSignalNames) {
  const std::array<SignalName, 1> supported_signals{
      {SignalName::kFileSystemInfo}};

  const auto names_set = signal_collector_.GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Tests that an unsupported signal is marked as unsupported.
TEST_F(FileSystemSignalsCollectorTest, GetSignal_Unsupported) {
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_.GetSignal(signal_name, CreateRequest(signal_name), response,
                              run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
}

// Tests that the request does not contain the required parameters for the
// File System signal.
TEST_F(FileSystemSignalsCollectorTest, GetSignal_File_MissingParameters) {
  SignalName signal_name = SignalName::kFileSystemInfo;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_.GetSignal(
      signal_name, CreateRequest(signal_name, /*with_file_parameter=*/false),
      response, run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.file_system_info_response.has_value());
  ASSERT_TRUE(response.file_system_info_response->collection_error.has_value());
  EXPECT_EQ(response.file_system_info_response->collection_error.value(),
            SignalCollectionError::kMissingParameters);
}

// Tests that not being able to retrieve a pointer to the SystemSignalsService
// returns an error.
TEST_F(FileSystemSignalsCollectorTest,
       GetSignal_File_MissingSystemSignalsService) {
  EXPECT_CALL(service_host_, GetService()).WillOnce(Return(nullptr));

  SignalName signal_name = SignalName::kFileSystemInfo;
  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_.GetSignal(signal_name, CreateRequest(signal_name), response,
                              run_loop.QuitClosure());

  run_loop.Run();

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.file_system_info_response.has_value());
  ASSERT_TRUE(response.file_system_info_response->collection_error.has_value());
  EXPECT_EQ(response.file_system_info_response->collection_error.value(),
            SignalCollectionError::kMissingSystemService);
}

// Tests a successful File System signal retrieval.
TEST_F(FileSystemSignalsCollectorTest, GetSignal_FileSystemInfo) {
  // Can be any value really.
  FileSystemItem retrieved_item;
  retrieved_item.file_path =
      base::FilePath::FromUTF8Unsafe("some successful file path");
  retrieved_item.presence = PresenceValue::kFound;

  std::vector<FileSystemItem> file_system_items;
  file_system_items.push_back(retrieved_item);

  SignalName signal_name = SignalName::kFileSystemInfo;
  auto request = CreateRequest(signal_name);

  EXPECT_CALL(service_host_, GetService()).Times(1);
  EXPECT_CALL(service_,
              GetFileSystemSignals(
                  ContainerEq(request.file_system_signal_parameters), _))
      .WillOnce(Invoke(
          [&file_system_items](
              const std::vector<GetFileSystemInfoOptions> signal_parameters,
              GetFileSystemSignalsCallback signal_callback) {
            std::move(signal_callback).Run(file_system_items);
          }));

  SignalsAggregationResponse response;
  base::RunLoop run_loop;
  signal_collector_.GetSignal(signal_name, request, response,
                              run_loop.QuitClosure());

  run_loop.Run();

  EXPECT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.file_system_info_response.has_value());
  EXPECT_FALSE(
      response.file_system_info_response->collection_error.has_value());
  EXPECT_EQ(response.file_system_info_response->file_system_items.size(),
            file_system_items.size());
  EXPECT_EQ(response.file_system_info_response->file_system_items[0],
            file_system_items[0]);
}

}  // namespace device_signals
