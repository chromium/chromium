// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/file_stream_data_pipe_getter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/open_file_system_mode.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace {

const char kURLOrigin[] = "http://remote/";
constexpr size_t kTestDataSize = 3 * 1024 * 1024;
constexpr int kBufSize = 32 * 1024;

// Reads the response until the channel is closed.
std::string ReadResponse(const mojo::DataPipeConsumerHandle& consumer,
                         uint64_t expected_size) {
  std::string result;
  while (true) {
    const void* buffer;
    uint32_t num_bytes;
    MojoResult rv =
        consumer.BeginReadData(&buffer, &num_bytes, MOJO_READ_DATA_FLAG_NONE);
    // If no data has been received yet, spin the message loop until it has.
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      mojo::SimpleWatcher watcher(FROM_HERE,
                                  mojo::SimpleWatcher::ArmingPolicy::AUTOMATIC,
                                  base::SequencedTaskRunnerHandle::Get());
      base::RunLoop run_loop;
      watcher.Watch(
          consumer,
          MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
          MOJO_WATCH_CONDITION_SATISFIED,
          base::BindRepeating(
              [](base::RepeatingClosure quit, MojoResult result,
                 const mojo::HandleSignalsState& state) { quit.Run(); },
              run_loop.QuitClosure()));
      run_loop.Run();
      continue;
    }

    // The pipe was closed.
    if (rv == MOJO_RESULT_FAILED_PRECONDITION) {
      EXPECT_EQ(result.size(), expected_size);
      return result;
    }

    CHECK_EQ(rv, MOJO_RESULT_OK);
    result.append(static_cast<const char*>(buffer), num_bytes);
    consumer.EndReadData(num_bytes);
  }
}

}  // namespace

class FileStreamDataPipeGetterTest : public testing::Test {
 public:
  FileStreamDataPipeGetterTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        test_data_(base::RandBytesAsString(kTestDataSize)) {}
  ~FileStreamDataPipeGetterTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath temp_path = temp_dir_.GetPath();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, temp_path, base::ThreadTaskRunnerHandle::Get(),
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>());
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), base::ThreadTaskRunnerHandle::Get());
    file_system_context_ = storage::CreateFileSystemContextForTesting(
        quota_manager_proxy_.get(), temp_path);
    base::RunLoop run_loop;
    file_system_context_->OpenFileSystem(
        blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
        /*bucket=*/absl::nullopt, storage::kFileSystemTypeTemporary,
        storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
        base::BindLambdaForTesting(
            [&run_loop](const storage::FileSystemURL& root_url,
                        const std::string& name, base::File::Error result) {
              ASSERT_EQ(base::File::FILE_OK, result);
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  std::string GetDataPipeUploadData(
      std::vector<mojo::PendingRemote<network::mojom::DataPipeGetter>>
          data_pipe_getters) {
    std::string result;
    for (mojo::PendingRemote<network::mojom::DataPipeGetter>&
             pending_data_pipe_getter : data_pipe_getters) {
      mojo::Remote<network::mojom::DataPipeGetter> data_pipe_getter;
      data_pipe_getter.Bind(std::move(pending_data_pipe_getter));
      EXPECT_TRUE(data_pipe_getter);

      mojo::ScopedDataPipeProducerHandle data_pipe_producer;
      mojo::ScopedDataPipeConsumerHandle data_pipe_consumer;
      base::RunLoop run_loop;
      EXPECT_EQ(MOJO_RESULT_OK,
                mojo::CreateDataPipe(nullptr, data_pipe_producer,
                                     data_pipe_consumer));
      uint64_t element_size;
      data_pipe_getter->Read(
          std::move(data_pipe_producer),
          base::BindLambdaForTesting(
              [&run_loop, &element_size](int32_t status, uint64_t size) {
                EXPECT_EQ(net::OK, status);
                element_size = size;
                run_loop.Quit();
              }));
      data_pipe_getter.FlushForTesting();
      run_loop.Run();

      EXPECT_TRUE(data_pipe_consumer.is_valid());

      result += ReadResponse(data_pipe_consumer.get(), element_size);
    }
    return result;
  }

 protected:
  storage::FileSystemURL CreateTestFile(const std::string& name,
                                        size_t offset,
                                        size_t file_size) {
    DCHECK(offset + file_size <= test_data_.size());
    // Setup a test file in the file system with random data.
    storage::FileSystemURL url =
        file_system_context_->CreateCrackedFileSystemURL(
            blink::StorageKey::CreateFromStringForTesting(kURLOrigin),
            storage::kFileSystemTypeTemporary,
            base::FilePath().AppendASCII(name));

    EXPECT_EQ(base::File::FILE_OK,
              storage::AsyncFileTestHelper::CreateFileWithData(
                  file_system_context_.get(), url, test_data_.data() + offset,
                  file_size));
    return url;
  }

  content::BrowserTaskEnvironment task_environment_;
  const std::string test_data_;

  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
};

TEST_F(FileStreamDataPipeGetterTest, SingleFile) {
  std::vector<mojo::PendingRemote<network::mojom::DataPipeGetter>>
      data_pipe_getter_remotes(1);
  {
    storage::FileSystemURL url = CreateTestFile("test.dat", 0, kTestDataSize);
    web_app::FileStreamDataPipeGetter::Create(
        /*receiver=*/data_pipe_getter_remotes[0]
            .InitWithNewPipeAndPassReceiver(),
        file_system_context_, url,
        /*offset=*/0,
        /*file_size=*/kTestDataSize,
        /*buf_size=*/kBufSize);
  }

  std::string response_body =
      GetDataPipeUploadData(std::move(data_pipe_getter_remotes));
  EXPECT_EQ(test_data_, response_body);
}

TEST_F(FileStreamDataPipeGetterTest, MultipleFiles) {
  constexpr int kNumFiles = 5;
  std::vector<mojo::PendingRemote<network::mojom::DataPipeGetter>>
      data_pipe_getter_remotes(kNumFiles);
  for (int index = 0; index < kNumFiles; ++index) {
    const size_t begin_offset = kTestDataSize * index / kNumFiles;
    const size_t end_offset = kTestDataSize * (index + 1) / kNumFiles;
    const size_t file_size = end_offset - begin_offset;

    storage::FileSystemURL url = CreateTestFile(
        base::StringPrintf("test%i.dat", index), begin_offset, file_size);

    web_app::FileStreamDataPipeGetter::Create(
        /*receiver=*/data_pipe_getter_remotes[index]
            .InitWithNewPipeAndPassReceiver(),
        file_system_context_, url,
        /*offset=*/0,
        /*file_size=*/file_size,
        /*buf_size=*/kBufSize);
  }

  std::string response_body =
      GetDataPipeUploadData(std::move(data_pipe_getter_remotes));
  EXPECT_EQ(test_data_, response_body);
}
