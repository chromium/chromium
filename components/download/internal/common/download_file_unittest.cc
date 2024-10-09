// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/mock_input_stream.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::SetArgPointee;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace download {
namespace {

// Struct for SourceStream states verification.
struct SourceStreamTestData {
  SourceStreamTestData(int64_t offset, int64_t bytes_written, bool finished)
      : offset(offset), bytes_written(bytes_written), finished(finished) {}
  int64_t offset;
  int64_t bytes_written;
  bool finished;
};

int64_t GetBuffersLength(const char** buffers, size_t num_buffer) {
  int64_t result = 0;
  for (size_t i = 0; i < num_buffer; ++i)
    result += static_cast<int64_t>(strlen(buffers[i]));
  return result;
}

std::string GetHexEncodedHashValue(crypto::SecureHash* hash_state) {
  if (!hash_state)
    return std::string();
  std::vector<uint8_t> hash_value(hash_state->GetHashLength());
  hash_state->Finish(&hash_value.front(), hash_value.size());
  return base::HexEncode(hash_value);
}

class MockDownloadDestinationObserver : public DownloadDestinationObserver {
 public:
  MOCK_METHOD3(DestinationUpdate,
               void(int64_t,
                    int64_t,
                    const std::vector<DownloadItem::ReceivedSlice>&));
  void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) override {
    MockDestinationError(reason, bytes_so_far,
                         GetHexEncodedHashValue(hash_state.get()));
  }
  void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) override {
    MockDestinationCompleted(total_bytes,
                             GetHexEncodedHashValue(hash_state.get()));
  }

  MOCK_METHOD3(MockDestinationError,
               void(DownloadInterruptReason, int64_t, const std::string&));
  MOCK_METHOD2(MockDestinationCompleted, void(int64_t, const std::string&));

  // Doesn't override any methods in the base class.  Used to make sure
  // that the last DestinationUpdate before a Destination{Completed,Error}
  // had the right values.
  MOCK_METHOD2(CurrentUpdateStatus, void(int64_t, int64_t));
};

enum DownloadFileRenameMethodType { RENAME_AND_UNIQUIFY, RENAME_AND_ANNOTATE };

// This is a test DownloadFileImpl that has no retry delay and, on Posix,
// retries renames failed due to ACCESS_DENIED.
class TestDownloadFileImpl : public DownloadFileImpl {
 public:
  TestDownloadFileImpl(std::unique_ptr<DownloadSaveInfo> save_info,
                       const base::FilePath& default_downloads_directory,
                       std::unique_ptr<InputStream> stream,
                       uint32_t download_id,
                       base::WeakPtr<DownloadDestinationObserver> observer)
      : DownloadFileImpl(std::move(save_info),
                         default_downloads_directory,
                         std::move(stream),
                         download_id,
                         observer) {}

 protected:
  base::TimeDelta GetRetryDelayForFailedRename(int attempt_count) override {
    return base::Milliseconds(0);
  }

#if !BUILDFLAG(IS_WIN)
  // On Posix, we don't encounter transient errors during renames, except
  // possibly EAGAIN, which is difficult to replicate reliably. So we resort to
  // simulating a transient error using ACCESS_DENIED instead.
  bool ShouldRetryFailedRename(DownloadInterruptReason reason) override {
    return reason == DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED;
  }
#endif
};

class MockQuarantine : public quarantine::mojom::Quarantine {
 public:
  MOCK_METHOD(void,
              QuarantineFile,
              (const base::FilePath& full_path,
               const GURL& source_url,
               const GURL& referrer_url,
               const std::optional<url::Origin>& request_initiator,
               const std::string& client_guid,
               quarantine::mojom::Quarantine::QuarantineFileCallback callback));
};

}  // namespace

class DownloadFileTest : public testing::Test {
 public:
  static const char kTestData1[];
  static const char kTestData2[];
  static const char kTestData3[];
  static const char kTestData4[];
  static const char kTestData5[];
  static const char* kTestData6[];
  static const char* kTestData7[];
  static const char* kTestData8[];
  static const char kDataHash[];
  static const char kEmptyHash[];
  static const uint32_t kDummyDownloadId;
  static const int kDummyChildId;
  static const int kDummyRequestId;

  DownloadFileTest()
      : observer_(new StrictMock<MockDownloadDestinationObserver>),
        observer_factory_(observer_.get()),
        input_stream_(nullptr),
        additional_streams_(std::vector<raw_ptr<StrictMock<MockInputStream>>>{
            nullptr, nullptr}),
        bytes_(-1),
        bytes_per_sec_(-1),
        quarantine_remote_(&quarantine_) {}

  ~DownloadFileTest() override = default;

  void SetUpdateDownloadInfo(
      int64_t bytes,
      int64_t bytes_per_sec,
      const std::vector<DownloadItem::ReceivedSlice>& received_slices) {
    bytes_ = bytes;
    bytes_per_sec_ = bytes_per_sec;
  }

  void ConfirmUpdateDownloadInfo() {
    observer_->CurrentUpdateStatus(bytes_, bytes_per_sec_);
  }

  void SetUp() override {
#if BUILDFLAG(IS_WIN)
    ASSERT_TRUE(com_initializer_.Succeeded());
#endif
    EXPECT_CALL(*(observer_.get()), DestinationUpdate(_, _, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(this, &DownloadFileTest::SetUpdateDownloadInfo));
    ON_CALL(quarantine_, QuarantineFile(_, _, _, _, _, _))
        .WillByDefault(WithArg<5>(
            [](quarantine::mojom::Quarantine::QuarantineFileCallback callback) {
              std::move(callback).Run(
                  quarantine::mojom::QuarantineFileResult::OK);
            }));
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }

  // Mock calls to this function are forwarded here.
  void RegisterCallback(
      const mojo::SimpleWatcher::ReadyCallback& sink_callback) {
    sink_callback_ = sink_callback;
  }

  void ClearCallback() { sink_callback_.Reset(); }

  void OnStreamActive(int64_t offset) {
    DCHECK(download_file_->source_streams_.find(offset) !=
           download_file_->source_streams_.end())
        << "Can't find stream at offset : " << offset;
    DownloadFileImpl::SourceStream* stream =
        download_file_->source_streams_[offset].get();
    download_file_->StreamActive(stream, MOJO_RESULT_OK);
  }

  void SetInterruptReasonCallback(base::OnceClosure closure,
                                  DownloadInterruptReason* reason_p,
                                  DownloadInterruptReason reason,
                                  int64_t bytes_wasted) {
    *reason_p = reason;
    std::move(closure).Run();
  }

  bool CreateDownloadFile(bool calculate_hash) {
    return CreateDownloadFile(0, calculate_hash, DownloadItem::ReceivedSlices(),
                              -1);
  }

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  bool CreateDownloadFile(int length, bool needs_obfuscation) {
    return CreateDownloadFile(length, true, DownloadItem::ReceivedSlices(), -1,
                              needs_obfuscation);
  }
#endif

  bool CreateDownloadFile(int length,
                          bool calculate_hash,
                          const DownloadItem::ReceivedSlices& received_slices) {
    return CreateDownloadFile(length, calculate_hash,
                              DownloadItem::ReceivedSlices(), -1);
  }

  bool CreateDownloadFile(int length,
                          bool calculate_hash,
                          const DownloadItem::ReceivedSlices& received_slices,
                          int file_offset,
                          bool needs_obfuscation = false) {
    // There can be only one.
    DCHECK(!download_file_);

    input_stream_ = new StrictMock<MockInputStream>();

    // TODO: Need to actually create a function that'll set the variables
    // based on the inputs from the callback.
    EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
        .WillOnce(Invoke(this, &DownloadFileTest::RegisterCallback))
        .RetiresOnSaturation();

    std::unique_ptr<DownloadSaveInfo> save_info(new DownloadSaveInfo());
    // Fill the file by repeatedly copying |kTestData1| if |file_offset| is
    // positive.
    if (file_offset > 0) {
      base::CreateTemporaryFileInDir(download_dir_.GetPath(),
                                     &save_info->file_path);
      int len = file_offset;
      int data_len = strlen(kTestData1);
      while (len > 0) {
        int bytes_to_write = len > data_len ? data_len : len;
        base::AppendToFile(save_info->file_path,
                           std::string_view(kTestData1, bytes_to_write));
        len -= bytes_to_write;
      }
    }

    save_info->offset = 0;
    save_info->file_offset = file_offset;
    if (needs_obfuscation) {
      save_info->needs_obfuscation = needs_obfuscation;
      save_info->total_bytes = length;
    }

    download_file_ = std::make_unique<TestDownloadFileImpl>(
        std::move(save_info), download_dir_.GetPath(),
        std::unique_ptr<MockInputStream>(input_stream_),
        DownloadItem::kInvalidId, observer_factory_.GetWeakPtr());

    EXPECT_CALL(*input_stream_, Read(_, _))
        .WillOnce(Return(InputStream::EMPTY))
        .RetiresOnSaturation();

    base::WeakPtrFactory<DownloadFileTest> weak_ptr_factory(this);
    DownloadInterruptReason result = DOWNLOAD_INTERRUPT_REASON_NONE;
    base::RunLoop loop_runner;
    download_file_->SetTaskRunnerForTesting(
        base::SequencedTaskRunner::GetCurrentDefault());
    download_file_->Initialize(
        base::BindRepeating(&DownloadFileTest::SetInterruptReasonCallback,
                            weak_ptr_factory.GetWeakPtr(),
                            loop_runner.QuitClosure(), &result),
        DownloadFile::CancelRequestCallback(), received_slices);
    loop_runner.Run();

    ::testing::Mock::VerifyAndClearExpectations(input_stream_);
    return result == DOWNLOAD_INTERRUPT_REASON_NONE;
  }

  void DestroyDownloadFile(int offset, bool compare_disk_data = true) {
    EXPECT_FALSE(download_file_->InProgress());

    // Make sure the data has been properly written to disk.
    if (compare_disk_data) {
      std::string disk_data;
      EXPECT_TRUE(
          base::ReadFileToString(download_file_->FullPath(), &disk_data));
      EXPECT_EQ(expected_data_, disk_data);
    }

    // Clear `raw_ptr`s before they become dangling pointers after resetting
    // `download_file_` below.
    input_stream_ = nullptr;
    additional_streams_.clear();

    // Make sure the Browser and File threads outlive the DownloadFile
    // to satisfy thread checks inside it.
    download_file_.reset();
  }

  // Setup the stream to append data or write from |offset| to the file.
  // Don't actually trigger the callback or do verifications.
  void SetupDataAppend(const char** data_chunks,
                       size_t num_chunks,
                       MockInputStream* input_stream,
                       ::testing::Sequence s,
                       int64_t offset = -1) {
    DCHECK(input_stream);
    size_t current_pos = static_cast<size_t>(offset);
    for (size_t i = 0; i < num_chunks; i++) {
      const char* source_data = data_chunks[i];
      size_t length = strlen(source_data);
      auto data = base::MakeRefCounted<net::IOBufferWithSize>(length);
      memcpy(data->data(), source_data, length);
      EXPECT_CALL(*input_stream, Read(_, _))
          .InSequence(s)
          .WillOnce(DoAll(SetArgPointee<0>(data), SetArgPointee<1>(length),
                          Return(InputStream::HAS_DATA)))
          .RetiresOnSaturation();

      if (offset < 0) {
        // Append data.
        expected_data_ += source_data;
        continue;
      }

      // Write from offset. May fill holes with '\0'.
      size_t new_len = current_pos + length;
      if (new_len > expected_data_.size())
        expected_data_.append(new_len - expected_data_.size(), '\0');
      expected_data_.replace(current_pos, length, source_data);
      current_pos += length;
    }
  }

  void VerifyStreamAndSize() {
    ::testing::Mock::VerifyAndClearExpectations(input_stream_);
    int64_t size;
    EXPECT_TRUE(base::GetFileSize(download_file_->FullPath(), &size));
    EXPECT_EQ(expected_data_.size(), static_cast<size_t>(size));
  }

  // TODO(rdsmith): Manage full percentage issues properly.
  void AppendDataToFile(const char** data_chunks, size_t num_chunks) {
    ::testing::Sequence s1;
    SetupDataAppend(data_chunks, num_chunks, input_stream_, s1);
    EXPECT_CALL(*input_stream_, Read(_, _))
        .InSequence(s1)
        .WillOnce(Return(InputStream::EMPTY))
        .RetiresOnSaturation();
    sink_callback_.Run(MOJO_RESULT_OK);
    VerifyStreamAndSize();
  }

  void SetupFinishStream(DownloadInterruptReason interrupt_reason,
                         MockInputStream* input_stream,
                         ::testing::Sequence s) {
    EXPECT_CALL(*input_stream, Read(_, _))
        .InSequence(s)
        .WillOnce(Return(InputStream::COMPLETE))
        .RetiresOnSaturation();
    EXPECT_CALL(*input_stream, GetCompletionStatus())
        .InSequence(s)
        .WillOnce(Return(interrupt_reason))
        .RetiresOnSaturation();
    EXPECT_CALL(*input_stream, ClearDataReadyCallback()).RetiresOnSaturation();
  }

  void FinishStream(DownloadInterruptReason interrupt_reason,
                    bool check_observer,
                    const std::string& expected_hash) {
    ::testing::Sequence s1;
    SetupFinishStream(interrupt_reason, input_stream_, s1);
    sink_callback_.Run(MOJO_RESULT_OK);
    VerifyStreamAndSize();
    if (check_observer) {
      EXPECT_CALL(*(observer_.get()),
                  MockDestinationCompleted(_, expected_hash));
      base::RunLoop().RunUntilIdle();
      ::testing::Mock::VerifyAndClearExpectations(observer_.get());
      EXPECT_CALL(*(observer_.get()), DestinationUpdate(_, _, _))
          .Times(AnyNumber())
          .WillRepeatedly(
              Invoke(this, &DownloadFileTest::SetUpdateDownloadInfo));
    }
  }

  DownloadInterruptReason RenameAndUniquify(const base::FilePath& full_path,
                                            base::FilePath* result_path_p) {
    return InvokeRenameMethodAndWaitForCallback(RENAME_AND_UNIQUIFY, full_path,
                                                result_path_p);
  }

  DownloadInterruptReason RenameAndAnnotate(const base::FilePath& full_path,
                                            base::FilePath* result_path_p) {
    return InvokeRenameMethodAndWaitForCallback(RENAME_AND_ANNOTATE, full_path,
                                                result_path_p);
  }

  void ExpectPermissionError(DownloadInterruptReason err) {
    EXPECT_TRUE(err == DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR ||
                err == DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED)
        << "Interrupt reason = " << err;
  }

 protected:
  void InvokeRenameMethod(
      DownloadFileRenameMethodType method,
      const base::FilePath& full_path,
      DownloadFile::RenameCompletionCallback completion_callback) {
    switch (method) {
      case RENAME_AND_UNIQUIFY:
        download_file_->RenameAndUniquify(full_path,
                                          std::move(completion_callback));
        break;

      case RENAME_AND_ANNOTATE:
        // We cannot rebind a mojo::Remote without resetting it. The
        // real implementation binds a new Remote on every call to
        // RenameAndAnnotate, but it's simpler to reuse
        // `quarantine_remote_` in tests.
        quarantine_remote_.reset();
        download_file_->RenameAndAnnotate(
            full_path, "12345678-ABCD-1234-DCBA-123456789ABC",
            GURL("https://source.example.com/"),
            GURL("https://referrer.example.com/"),
            /*request_initiator=*/
            url::Origin::Create(GURL("https://initiator.example.com/")),
            quarantine_remote_.BindNewPipeAndPassRemote(),
            std::move(completion_callback));
        break;
    }
  }

  DownloadInterruptReason InvokeRenameMethodAndWaitForCallback(
      DownloadFileRenameMethodType method,
      const base::FilePath& full_path,
      base::FilePath* result_path_p) {
    DownloadInterruptReason result_reason(DOWNLOAD_INTERRUPT_REASON_NONE);
    base::FilePath result_path;
    base::RunLoop loop_runner;
    DownloadFile::RenameCompletionCallback completion_callback = base::BindOnce(
        &DownloadFileTest::SetRenameResult, base::Unretained(this),
        loop_runner.QuitClosure(), &result_reason, result_path_p);
    InvokeRenameMethod(method, full_path, std::move(completion_callback));
    loop_runner.Run();
    return result_reason;
  }

  // Prepare a byte stream to write to the file sink.
  void PrepareStream(raw_ptr<StrictMock<MockInputStream>>* stream,
                     int64_t offset,
                     bool create_stream,
                     bool will_finish,
                     const char** buffers,
                     size_t num_buffer) {
    if (create_stream)
      *stream = new StrictMock<MockInputStream>();

    // Expectation on MockInputStream for MultipleStreams tests:
    // 1. RegisterCallback: Must called twice. One to set the callback, the
    // other to release the stream.
    // 2. Read: If filled with N buffer, called (N+1) times, where the last Read
    // call doesn't read any data but returns STREAM_COMPLETE.
    // The stream may terminate in the middle and less Read calls are expected.
    // 3. GetStatus: Only called if the stream is completed and last Read call
    // returns STREAM_COMPLETE.
    Sequence seq;
    SetupDataAppend(buffers, num_buffer, *stream, seq, offset);
    if (will_finish)
      SetupFinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, *stream, seq);
  }

  void VerifySourceStreamsStates(const SourceStreamTestData& data) {
    DCHECK(download_file_->source_streams_.find(data.offset) !=
           download_file_->source_streams_.end())
        << "Can't find stream at offset : " << data.offset;
    DownloadFileImpl::SourceStream* stream =
        download_file_->source_streams_[data.offset].get();
    DCHECK(stream);
    EXPECT_EQ(data.offset, stream->offset());
    EXPECT_EQ(data.bytes_written, stream->bytes_written());
    EXPECT_EQ(data.finished, stream->is_finished());
  }

  size_t source_streams_count() const {
    DCHECK(download_file_);
    return download_file_->source_streams_.size();
  }

  int64_t TotalBytesReceived() const {
    DCHECK(download_file_);
    return download_file_->TotalBytesReceived();
  }

 private:
#if BUILDFLAG(IS_WIN)
  // This must occur early in the member list to ensure COM is initialized first
  // and uninitialized last.
  base::win::ScopedCOMInitializer com_initializer_;
#endif

 protected:
  std::unique_ptr<StrictMock<MockDownloadDestinationObserver>> observer_;
  base::WeakPtrFactory<DownloadDestinationObserver> observer_factory_;

  // DownloadFile instance we are testing.
  std::unique_ptr<DownloadFileImpl> download_file_;

  // Stream for sending data into the download file.
  // Owned by download_file_; will be alive for lifetime of download_file_.
  raw_ptr<StrictMock<MockInputStream>> input_stream_;

  // Additional streams to test multiple stream write.
  std::vector<raw_ptr<StrictMock<MockInputStream>>> additional_streams_;

  // Sink callback data for stream.
  mojo::SimpleWatcher::ReadyCallback sink_callback_;

  base::ScopedTempDir download_dir_;

  // Latest update sent to the observer.
  int64_t bytes_;
  int64_t bytes_per_sec_;

  // Keep track of what data should be saved to the disk file.
  std::string expected_data_;

  MockQuarantine quarantine_;

 private:
  void SetRenameResult(base::OnceClosure closure,
                       DownloadInterruptReason* reason_p,
                       base::FilePath* result_path_p,
                       DownloadInterruptReason reason,
                       const base::FilePath& result_path) {
    if (reason_p)
      *reason_p = reason;
    if (result_path_p)
      *result_path_p = result_path;
    std::move(closure).Run();
  }

  mojo::Receiver<quarantine::mojom::Quarantine> quarantine_remote_;

  base::test::TaskEnvironment task_environment_;
};

// DownloadFile::RenameAndAnnotate and DownloadFile::RenameAndUniquify have a
// considerable amount of functional overlap. In order to re-use test logic, we
// are going to introduce this value parameterized test fixture. It will take a
// DownloadFileRenameMethodType value which can be either of the two rename
// methods.
class DownloadFileTestWithRename
    : public DownloadFileTest,
      public ::testing::WithParamInterface<DownloadFileRenameMethodType> {
 protected:
  DownloadInterruptReason InvokeSelectedRenameMethod(
      const base::FilePath& full_path,
      base::FilePath* result_path_p) {
    return InvokeRenameMethodAndWaitForCallback(GetParam(), full_path,
                                                result_path_p);
  }
};

// And now instantiate all DownloadFileTestWithRename tests using both
// DownloadFile rename methods. Each test of the form
// DownloadFileTestWithRename.<FooTest> will be instantiated once with
// RenameAndAnnotate as the value parameter and once with RenameAndUniquify as
// the value parameter.
INSTANTIATE_TEST_SUITE_P(DownloadFile,
                         DownloadFileTestWithRename,
                         ::testing::Values(RENAME_AND_ANNOTATE,
                                           RENAME_AND_UNIQUIFY));

const char DownloadFileTest::kTestData1[] =
    "Let's write some data to the file!\n";
const char DownloadFileTest::kTestData2[] = "Writing more data.\n";
const char DownloadFileTest::kTestData3[] = "Final line.";
const char DownloadFileTest::kTestData4[] = "abcdefg";
const char DownloadFileTest::kTestData5[] = "01234";
const char* DownloadFileTest::kTestData6[] = {kTestData1, kTestData2};
const char* DownloadFileTest::kTestData7[] = {kTestData4, kTestData5};
const char* DownloadFileTest::kTestData8[] = {kTestData1, kTestData2,
                                              kTestData4, kTestData5};

const char DownloadFileTest::kDataHash[] =
    "CBF68BF10F8003DB86B31343AFAC8C7175BD03FB5FC905650F8C80AF087443A8";
const char DownloadFileTest::kEmptyHash[] =
    "E3B0C44298FC1C149AFBF4C8996FB92427AE41E4649B934CA495991B7852B855";

const uint32_t DownloadFileTest::kDummyDownloadId = 23;
const int DownloadFileTest::kDummyChildId = 3;
const int DownloadFileTest::kDummyRequestId = 67;

// Rename the file before any data is downloaded, after some has, after it all
// has, and after it's closed.
TEST_P(DownloadFileTestWithRename, RenameFileFinal) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));
  base::FilePath path_2(initial_path.InsertBeforeExtensionASCII("_2"));
  base::FilePath path_3(initial_path.InsertBeforeExtensionASCII("_3"));
  base::FilePath path_4(initial_path.InsertBeforeExtensionASCII("_4"));
  base::FilePath output_path;

  // Rename the file before downloading any data.
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            InvokeSelectedRenameMethod(path_1, &output_path));
  base::FilePath renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_1, renamed_path);
  EXPECT_EQ(path_1, output_path);

  // Check the files.
  EXPECT_FALSE(base::PathExists(initial_path));
  EXPECT_TRUE(base::PathExists(path_1));

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();
  // Download the data.
  const char* chunks1[] = {kTestData1, kTestData2};
  AppendDataToFile(chunks1, 2);

  // Rename the file after downloading some data.
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            InvokeSelectedRenameMethod(path_2, &output_path));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_2, renamed_path);
  EXPECT_EQ(path_2, output_path);

  // Check the files.
  EXPECT_FALSE(base::PathExists(path_1));
  EXPECT_TRUE(base::PathExists(path_2));

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();
  const char* chunks2[] = {kTestData3};
  AppendDataToFile(chunks2, 1);

  // Rename the file after downloading all the data.
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            InvokeSelectedRenameMethod(path_3, &output_path));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_3, renamed_path);
  EXPECT_EQ(path_3, output_path);

  // Check the files.
  EXPECT_FALSE(base::PathExists(path_2));
  EXPECT_TRUE(base::PathExists(path_3));

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kDataHash);
  base::RunLoop().RunUntilIdle();

  // Rename the file after downloading all the data and closing the file.
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            InvokeSelectedRenameMethod(path_4, &output_path));
  renamed_path = download_file_->FullPath();
  EXPECT_EQ(path_4, renamed_path);
  EXPECT_EQ(path_4, output_path);

  // Check the files.
  EXPECT_FALSE(base::PathExists(path_3));
  EXPECT_TRUE(base::PathExists(path_4));

  DestroyDownloadFile(0);
}

// Test to make sure the rename overwrites when requested. This is separate from
// the above test because it only applies to RenameAndAnnotate().
// RenameAndUniquify() doesn't overwrite by design.
TEST_F(DownloadFileTest, RenameOverwrites) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));

  ASSERT_FALSE(base::PathExists(path_1));
  static const char file_data[] = "xyzzy";
  ASSERT_TRUE(base::WriteFile(path_1, file_data));
  ASSERT_TRUE(base::PathExists(path_1));

  base::FilePath new_path;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            RenameAndAnnotate(path_1, &new_path));
  EXPECT_EQ(path_1.value(), new_path.value());

  std::string file_contents;
  ASSERT_TRUE(base::ReadFileToString(new_path, &file_contents));
  EXPECT_NE(std::string(file_data), file_contents);

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

// Test to make sure the rename uniquifies if we aren't overwriting
// and there's a file where we're aiming. As above, not a
// DownloadFileTestWithRename test because this only applies to
// RenameAndUniquify().
TEST_F(DownloadFileTest, RenameUniquifies) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));
  base::FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));
  base::FilePath path_1_suffixed(path_1.InsertBeforeExtensionASCII(" (1)"));

  ASSERT_FALSE(base::PathExists(path_1));
  static const char file_data[] = "xyzzy";
  ASSERT_TRUE(base::WriteFile(path_1, file_data));
  ASSERT_TRUE(base::PathExists(path_1));

  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, RenameAndUniquify(path_1, nullptr));
  EXPECT_TRUE(base::PathExists(path_1_suffixed));

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

// Test that RenameAndUniquify doesn't try to uniquify in the case where the
// target filename is the same as the current filename.
TEST_F(DownloadFileTest, RenameRecognizesSelfConflict) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  base::FilePath new_path;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            RenameAndUniquify(initial_path, &new_path));
  EXPECT_TRUE(base::PathExists(initial_path));

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
  EXPECT_EQ(initial_path.value(), new_path.value());
}

#if BUILDFLAG(IS_MAC)
// Test that RenameAndUniquify will remove file hidden flag.
TEST_F(DownloadFileTest, RenameRemovesHiddenFlag) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));
  // Set the file hidden.
  base::stat_wrapper_t stat;
  base::File::Stat(initial_path, &stat);
  // Update the file's hidden flags.
  chflags(initial_path.value().c_str(), stat.st_flags | UF_HIDDEN);

  base::FilePath target_path =
      initial_path.DirName().Append(FILE_PATH_LITERAL("foo"));
  base::FilePath new_path;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            RenameAndUniquify(target_path, &new_path));
  EXPECT_TRUE(base::PathExists(target_path));
  base::File::Stat(initial_path, &stat);
  EXPECT_FALSE(stat.st_flags & UF_HIDDEN);

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();

  DestroyDownloadFile(0);
  EXPECT_EQ(target_path.value(), new_path.value());
}
#endif

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221273): Re-enable when RenameError works on Fuchsia.
#define MAYBE_RenameError DISABLED_RenameError
#else
#define MAYBE_RenameError RenameError
#endif
// Test to make sure we get the proper error on failure.
TEST_P(DownloadFileTestWithRename, MAYBE_RenameError) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());

  // Create a subdirectory.
  base::FilePath target_dir(
      initial_path.DirName().Append(FILE_PATH_LITERAL("TargetDir")));
  ASSERT_FALSE(base::DirectoryExists(target_dir));
  ASSERT_TRUE(base::CreateDirectory(target_dir));
  base::FilePath target_path(target_dir.Append(initial_path.BaseName()));

  // Targets
  base::FilePath target_path_suffixed(
      target_path.InsertBeforeExtensionASCII(" (1)"));
  ASSERT_FALSE(base::PathExists(target_path));
  ASSERT_FALSE(base::PathExists(target_path_suffixed));

  // Make the directory unwritable and try to rename within it.
  {
    base::FilePermissionRestorer restorer(target_dir);
    ASSERT_TRUE(base::MakeFileUnwritable(target_dir));

    // Expect nulling out of further processing.
    EXPECT_CALL(*input_stream_, ClearDataReadyCallback());
    ExpectPermissionError(InvokeSelectedRenameMethod(target_path, nullptr));
    EXPECT_FALSE(base::PathExists(target_path_suffixed));
  }

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

namespace {

void TestRenameCompletionCallback(base::OnceClosure closure,
                                  bool* did_run_callback,
                                  DownloadInterruptReason interrupt_reason,
                                  const base::FilePath& new_path) {
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, interrupt_reason);
  *did_run_callback = true;
  std::move(closure).Run();
}

}  // namespace

#if BUILDFLAG(IS_FUCHSIA)
// TODO(crbug.com/40221274): Re-enable when RenameWithErrorRetry works on
// Fuchsia.
#define MAYBE_RenameWithErrorRetry DISABLED_RenameWithErrorRetry
#else
#define MAYBE_RenameWithErrorRetry RenameWithErrorRetry
#endif
// Test that the retry logic works. This test assumes that DownloadFileImpl will
// post tasks to the current message loop (acting as the download sequence)
// asynchronously to retry the renames. We will stuff RunLoop::QuitClosures()
// in between the retry tasks to stagger them and then allow the rename to
// succeed.
//
// Note that there is only one queue of tasks to run, and that is in the tests'
// base::CurrentThread::Get(). Each RunLoop processes that queue until it
// sees a QuitClosure() targeted at itself, at which point it stops processing.
TEST_P(DownloadFileTestWithRename, MAYBE_RenameWithErrorRetry) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());

  // Create a subdirectory.
  base::FilePath target_dir(
      initial_path.DirName().Append(FILE_PATH_LITERAL("TargetDir")));
  ASSERT_FALSE(base::DirectoryExists(target_dir));
  ASSERT_TRUE(base::CreateDirectory(target_dir));
  base::FilePath target_path(target_dir.Append(initial_path.BaseName()));

  bool did_run_callback = false;

  // Each RunLoop can be used the run the MessageLoop until the corresponding
  // QuitClosure() is run. This one is used to produce the QuitClosure() that
  // will be run when the entire rename operation is complete.
  base::RunLoop succeeding_run;
  {
// (Scope for the base::File or base::FilePermissionRestorer below.)
#if BUILDFLAG(IS_WIN)
    // On Windows we test with an actual transient error, a sharing violation.
    // The rename will fail because we are holding the file open for READ. On
    // Posix this doesn't cause a failure.
    base::File locked_file(initial_path,
                           base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(locked_file.IsValid());
#else
    // Simulate a transient failure by revoking write permission for target_dir.
    // The TestDownloadFileImpl class treats this error as transient even though
    // DownloadFileImpl itself doesn't.
    base::FilePermissionRestorer restore_permissions_for(target_dir);
    ASSERT_TRUE(base::MakeFileUnwritable(target_dir));
#endif

    // The Rename() should fail here and enqueue a retry task without invoking
    // the completion callback.
    InvokeRenameMethod(
        GetParam(), target_path,
        base::BindOnce(&TestRenameCompletionCallback, succeeding_run.QuitClosure(),
                   &did_run_callback));
    EXPECT_FALSE(did_run_callback);

    base::RunLoop first_failing_run;
    // Queue the QuitClosure() on the MessageLoop now. Any tasks queued by the
    // Rename() will be in front of the QuitClosure(). Running the message loop
    // now causes the just the first retry task to be run. The rename still
    // fails, so another retry task would get queued behind the QuitClosure().
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, first_failing_run.QuitClosure());
    first_failing_run.Run();
    EXPECT_FALSE(did_run_callback);

    // Running another loop should have the same effect as the above as long as
    // kMaxRenameRetries is greater than 2.
    base::RunLoop second_failing_run;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, second_failing_run.QuitClosure());
    second_failing_run.Run();
    EXPECT_FALSE(did_run_callback);
  }

  // This time the QuitClosure from succeeding_run should get executed.
  succeeding_run.Run();
  EXPECT_TRUE(did_run_callback);

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

// Various tests of the StreamActive method.
TEST_F(DownloadFileTest, StreamEmptySuccess) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  // Test that calling the sink_callback_ on an empty stream shouldn't
  // do anything.
  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();
  AppendDataToFile(nullptr, 0);

  // Finish the download this way and make sure we see it on the observer.
  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();

  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamEmptyError) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  // Finish the download in error and make sure we see it on the
  // observer.
  EXPECT_CALL(
      *(observer_.get()),
      MockDestinationError(DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED, 0,
                           kEmptyHash))
      .WillOnce(InvokeWithoutArgs(
          this, &DownloadFileTest::ConfirmUpdateDownloadInfo));

  // If this next EXPECT_CALL fails flakily, it's probably a real failure.
  // We'll be getting a stream of UpdateDownload calls from the timer, and
  // the last one may have the correct information even if the failure
  // doesn't produce an update, as the timer update may have triggered at the
  // same time.
  EXPECT_CALL(*(observer_.get()), CurrentUpdateStatus(0, _));

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED, false,
               kEmptyHash);

  base::RunLoop().RunUntilIdle();

  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamNonEmptySuccess) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  const char* chunks1[] = {kTestData1, kTestData2};
  ::testing::Sequence s1;
  SetupDataAppend(chunks1, 2, input_stream_, s1);
  SetupFinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, input_stream_, s1);
  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));
  sink_callback_.Run(MOJO_RESULT_OK);
  VerifyStreamAndSize();
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTest, StreamNonEmptyError) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  const char* chunks1[] = {kTestData1, kTestData2};
  ::testing::Sequence s1;
  SetupDataAppend(chunks1, 2, input_stream_, s1);
  SetupFinishStream(DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED,
                    input_stream_, s1);

  EXPECT_CALL(*(observer_.get()),
              MockDestinationError(
                  DOWNLOAD_INTERRUPT_REASON_NETWORK_DISCONNECTED, _, _))
      .WillOnce(InvokeWithoutArgs(
          this, &DownloadFileTest::ConfirmUpdateDownloadInfo));

  // If this next EXPECT_CALL fails flakily, it's probably a real failure.
  // We'll be getting a stream of UpdateDownload calls from the timer, and
  // the last one may have the correct information even if the failure
  // doesn't produce an update, as the timer update may have triggered at the
  // same time.
  EXPECT_CALL(*(observer_.get()),
              CurrentUpdateStatus(strlen(kTestData1) + strlen(kTestData2), _));

  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();
  VerifyStreamAndSize();
  DestroyDownloadFile(0);
}

// Tests that if file content validation succeeds, all the remaining data will
// be writing to the file.
TEST_F(DownloadFileTest, FileContentValidationSuccess) {
  int stream_length = strlen(kTestData1) * 2;

  ASSERT_TRUE(CreateDownloadFile(
      stream_length /* length */, true /* calculate_hash */,
      DownloadItem::ReceivedSlices(), strlen(kTestData1) - 1));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));

  const char* chunks1[] = {kTestData1, kTestData1};
  ::testing::Sequence s1;
  SetupDataAppend(chunks1, 2 /* num_chunks */, input_stream_, s1);
  SetupFinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, input_stream_, s1);
  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));
  sink_callback_.Run(MOJO_RESULT_OK);
  VerifyStreamAndSize();
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

// Tests that if file content validation fails, an error will occur and no data
// will be written.
TEST_F(DownloadFileTest, FileContentValidationFail) {
  int file_length = strlen(kTestData2) - 1;
  int stream_length = strlen(kTestData1) + strlen(kTestData2);

  ASSERT_TRUE(CreateDownloadFile(stream_length /* length */,
                                 true /* calculate_hash */,
                                 DownloadItem::ReceivedSlices(), file_length));
  base::FilePath initial_path(download_file_->FullPath());
  EXPECT_TRUE(base::PathExists(initial_path));
  std::string file_content = std::string(kTestData1, 0, file_length);
  expected_data_ = file_content;
  VerifyStreamAndSize();

  const char* chunks1[] = {kTestData2, kTestData1};
  ::testing::Sequence s1;
  // Only 1 chunk will be read, and it will generate an error after
  // failing the validation.
  SetupDataAppend(chunks1, 1 /* num_chunks */, input_stream_, s1);
  EXPECT_CALL(*input_stream_, ClearDataReadyCallback());
  EXPECT_CALL(*(observer_.get()),
              MockDestinationError(DOWNLOAD_INTERRUPT_REASON_FILE_HASH_MISMATCH,
                                   file_length, _));
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();
  expected_data_ = file_content;
  DestroyDownloadFile(0);
}

// Tests for concurrent streams handling, used for parallel download.
//
// Activate both streams at the same time.
TEST_F(DownloadFileTest, MultipleStreamsWrite) {
  int64_t stream_0_length = GetBuffersLength(kTestData6, 2);
  int64_t stream_1_length = GetBuffersLength(kTestData7, 2);

  ASSERT_TRUE(CreateDownloadFile(stream_0_length, true,
                                 DownloadItem::ReceivedSlices()));

  PrepareStream(&input_stream_, 0, false, true, kTestData6, 2);
  PrepareStream(&additional_streams_[0], stream_0_length, true, true,
                kTestData7, 2);

  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));

  // Activate the streams.
  download_file_->AddInputStream(
      std::unique_ptr<MockInputStream>(additional_streams_[0]),
      stream_0_length);
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  SourceStreamTestData stream_data_0(0, stream_0_length, true);
  SourceStreamTestData stream_data_1(stream_0_length, stream_1_length, true);
  VerifySourceStreamsStates(stream_data_0);
  VerifySourceStreamsStates(stream_data_1);
  EXPECT_EQ(stream_0_length + stream_1_length, TotalBytesReceived());

  DestroyDownloadFile(0);
}

// 3 streams write to one sink, the second stream has a limited length.
TEST_F(DownloadFileTest, MultipleStreamsLimitedLength) {
  int64_t stream_0_length = GetBuffersLength(kTestData6, 2);

  // The second stream has a limited length and should be partially written
  // to disk. When we prepare the stream, we fill the stream with 2 full buffer.
  int64_t stream_1_length = GetBuffersLength(kTestData7, 2) - 1;

  // The last stream can't have length limit, it's a half open request, e.g
  // "Range:50-".
  int64_t stream_2_length = GetBuffersLength(kTestData6, 2);

  ASSERT_TRUE(CreateDownloadFile(stream_0_length, true,
                                 DownloadItem::ReceivedSlices()));

  PrepareStream(&input_stream_, 0, false, true, kTestData6, 2);
  PrepareStream(&additional_streams_[0], stream_0_length, true, false,
                kTestData7, 2);
  PrepareStream(&additional_streams_[1], stream_0_length + stream_1_length,
                true, true, kTestData6, 2);

  EXPECT_CALL(*additional_streams_[0], ClearDataReadyCallback())
      .Times(1)
      .RetiresOnSaturation();

  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));

  // Activate all the streams.
   download_file_->AddInputStream(
      std::unique_ptr<MockInputStream>(additional_streams_[1]),
      stream_0_length + stream_1_length);
  download_file_->AddInputStream(
      std::unique_ptr<MockInputStream>(additional_streams_[0]),
      stream_0_length);
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  SourceStreamTestData stream_data_0(0, stream_0_length, true);
  SourceStreamTestData stream_data_1(stream_0_length, stream_1_length, true);
  SourceStreamTestData stream_data_2(stream_0_length + stream_1_length,
                                     stream_2_length, true);

  VerifySourceStreamsStates(stream_data_0);
  VerifySourceStreamsStates(stream_data_1);
  VerifySourceStreamsStates(stream_data_2);

  EXPECT_EQ(stream_0_length + stream_1_length + stream_2_length,
            TotalBytesReceived());

  download_file_->Cancel();
  DestroyDownloadFile(0, false);
}

// Activate and deplete one stream, later add the second stream.
TEST_F(DownloadFileTest, MultipleStreamsFirstStreamWriteAllData) {
  int64_t stream_0_length = GetBuffersLength(kTestData8, 4);

  ASSERT_TRUE(CreateDownloadFile(DownloadSaveInfo::kLengthFullContent, true,
                                 DownloadItem::ReceivedSlices()));

  PrepareStream(&input_stream_, 0, false, true, kTestData8, 4);

  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));

  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  // Add another stream, the file is already closed, so nothing should be
  // called.
  EXPECT_FALSE(download_file_->InProgress());

  // Clear `raw_ptr`s before they become dangling pointers after the
  // `AddInputStream` call below.
  input_stream_ = nullptr;
  additional_streams_.clear();

  download_file_->AddInputStream(
      std::make_unique<StrictMock<MockInputStream>>(), stream_0_length - 1);
  base::RunLoop().RunUntilIdle();

  SourceStreamTestData stream_data_0(0, stream_0_length, true);
  VerifySourceStreamsStates(stream_data_0);
  EXPECT_EQ(stream_0_length, TotalBytesReceived());
  EXPECT_EQ(1u, source_streams_count());

  DestroyDownloadFile(0);
}

// While one stream is writing, kick off another stream with an offset that has
// been written by the first one.
TEST_F(DownloadFileTest, SecondStreamStartingOffsetAlreadyWritten) {
  int64_t stream_0_length = GetBuffersLength(kTestData6, 2);

  ASSERT_TRUE(CreateDownloadFile(stream_0_length, true,
                                 DownloadItem::ReceivedSlices()));

  Sequence seq;
  SetupDataAppend(kTestData6, 2, input_stream_, seq, 0);

  EXPECT_CALL(*input_stream_, Read(_, _))
      .InSequence(seq)
      .WillOnce(Return(InputStream::EMPTY))
      .RetiresOnSaturation();
  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  additional_streams_[0] = new StrictMock<MockInputStream>();
  EXPECT_CALL(*additional_streams_[0], ClearDataReadyCallback())
      .WillRepeatedly(Invoke(this, &DownloadFileTest::ClearCallback))
      .RetiresOnSaturation();
  EXPECT_CALL(*additional_streams_[0], Read(_, _))
      .WillOnce(Return(InputStream::EMPTY))
      .RetiresOnSaturation();

  download_file_->AddInputStream(
      std::unique_ptr<MockInputStream>(additional_streams_[0]),
      strlen(kTestData1));

  // The stream should get terminated and reset the callback.
  EXPECT_TRUE(sink_callback_.is_null());
  download_file_->Cancel();
  DestroyDownloadFile(0, false);
}

// The second stream successfully reads the data from its offset. However,
// before it is able to write the data, the same block was written by
// the first stream.
TEST_F(DownloadFileTest, SecondStreamReadsOffsetWrittenByFirst) {
  int64_t stream_0_length = GetBuffersLength(kTestData8, 4);

  ASSERT_TRUE(CreateDownloadFile(stream_0_length, true,
                                 DownloadItem::ReceivedSlices()));

  // First stream writes the first 2 chunks.
  Sequence seq;
  SetupDataAppend(kTestData8, 2, input_stream_, seq, 0);

  EXPECT_CALL(*input_stream_, Read(_, _))
      .InSequence(seq)
      .WillOnce(Return(InputStream::EMPTY))
      .RetiresOnSaturation();
  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(2)
      .RetiresOnSaturation();
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  // The second stream is created and waiting for data.
  additional_streams_[0] = new StrictMock<MockInputStream>();
  EXPECT_CALL(*additional_streams_[0], RegisterDataReadyCallback(_))
      .RetiresOnSaturation();
  EXPECT_CALL(*additional_streams_[0], ClearDataReadyCallback())
      .RetiresOnSaturation();
  EXPECT_CALL(*additional_streams_[0], Read(_, _))
      .WillOnce(Return(InputStream::EMPTY))
      .RetiresOnSaturation();
  int64_t offset = strlen(kTestData1) + strlen(kTestData2);
  download_file_->AddInputStream(
      std::unique_ptr<MockInputStream>(additional_streams_[0]), offset);
  base::RunLoop().RunUntilIdle();

  // First stream reads the 3rd chunk and writes it to disk.
  const char* chunk[] = {kTestData4};
  SetupDataAppend(chunk, 1, input_stream_, seq, offset);
  EXPECT_CALL(*input_stream_, Read(_, _))
      .InSequence(seq)
      .WillOnce(Return(InputStream::EMPTY))
      .RetiresOnSaturation();
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  // Second stream also reads the 3rd chunk, but it will be terminated.
  SetupDataAppend(chunk, 1, additional_streams_[0], seq, offset);
  OnStreamActive(offset);
  base::RunLoop().RunUntilIdle();

  // First stream writes the last chunk, and completes the download.
  chunk[0] = kTestData5;
  SetupDataAppend(chunk, 1, input_stream_, seq, offset + strlen(kTestData4));
  SetupFinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, input_stream_, seq);
  EXPECT_CALL(*(observer_.get()), MockDestinationCompleted(_, _));
  sink_callback_.Run(MOJO_RESULT_OK);
  base::RunLoop().RunUntilIdle();

  download_file_->Cancel();
  DestroyDownloadFile(0, false);
}

TEST_F(DownloadFileTest, PropagatesUrlAndInitiatorToQuarantine) {
  ASSERT_TRUE(CreateDownloadFile(true));
  base::FilePath initial_path(download_file_->FullPath());
  base::FilePath path_1(initial_path.InsertBeforeExtensionASCII("_1"));

  EXPECT_CALL(
      quarantine_,
      QuarantineFile(
          _, GURL("https://source.example.com/"),
          GURL("https://referrer.example.com"),
          Eq(url::Origin::Create(GURL("https://initiator.example.com/"))), _,
          _))
      .WillOnce(WithArg<5>(
          [](quarantine::mojom::Quarantine::QuarantineFileCallback callback) {
            std::move(callback).Run(
                quarantine::mojom::QuarantineFileResult::OK);
          }));
  base::FilePath new_path;
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            RenameAndAnnotate(path_1, &new_path));
  EXPECT_EQ(path_1.value(), new_path.value());

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kEmptyHash);
  base::RunLoop().RunUntilIdle();
  DestroyDownloadFile(0);
}

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class DownloadFileTestWithObfuscation : public DownloadFileTest {
 protected:
  // Constants for obfuscation overhead
  static constexpr size_t kObfuscationHeaderSize = 40;     // bytes
  static constexpr size_t kObfuscationChunkOverhead = 20;  // bytes per chunk

  void SetUp() override {
    DownloadFileTest::SetUp();
    scoped_feature_list_.InitAndEnableFeature(
        enterprise_obfuscation::kEnterpriseFileObfuscation);
  }

  size_t CalculateObfuscationOverhead(size_t num_chunks) const {
    return kObfuscationChunkOverhead * num_chunks + kObfuscationHeaderSize;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DownloadFileTestWithObfuscation, ObfuscationEnabled) {
  size_t length = strlen(kTestData1) + strlen(kTestData2) + strlen(kTestData3);
  ASSERT_TRUE(CreateDownloadFile(length, true));
  const char* chunks[] = {kTestData1, kTestData2, kTestData3};

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();

  // Append dummy data for obfuscated size verification.
  expected_data_ += std::string(CalculateObfuscationOverhead(3), '\0');
  AppendDataToFile(chunks, 3);

  // Original file hash should be returned, not the obfuscated hash.
  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kDataHash);

  // Verify that the file content is obfuscated.
  std::string file_content;
  ASSERT_TRUE(
      base::ReadFileToString(download_file_->FullPath(), &file_content));
  EXPECT_NE(file_content, std::string(kTestData1) + std::string(kTestData2) +
                              std::string(kTestData3));
  EXPECT_EQ(file_content.size(), length + CalculateObfuscationOverhead(3));

  download_file_->Cancel();
  DestroyDownloadFile(0, false);
}

TEST_F(DownloadFileTestWithObfuscation, ObfuscationWithUnknownFileSize) {
  size_t length = strlen(kTestData1) + strlen(kTestData2) + strlen(kTestData3);
  ASSERT_TRUE(CreateDownloadFile(0 /*Unknown file size*/, true));
  const char* chunks[] = {kTestData1, kTestData2, kTestData3};

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();

  // Append dummy data for obfuscated size verification.
  expected_data_ += std::string(CalculateObfuscationOverhead(3), '\0');
  AppendDataToFile(chunks, 3);

  // For files of unknown sizes, an empty chunk is appended once download
  // completes.
  expected_data_ += std::string(kObfuscationChunkOverhead, '\0');

  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kDataHash);

  // Verify that the file content is obfuscated, and includes an extra chunk.
  std::string file_content;
  ASSERT_TRUE(
      base::ReadFileToString(download_file_->FullPath(), &file_content));
  EXPECT_NE(file_content, std::string(kTestData1) + std::string(kTestData2) +
                              std::string(kTestData3));
  EXPECT_GT(file_content.size(), length);
  EXPECT_EQ(file_content.size(), length += CalculateObfuscationOverhead(4));

  download_file_->Cancel();
  DestroyDownloadFile(0, false);
}

TEST_F(DownloadFileTestWithObfuscation, ObfuscationDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(
      enterprise_obfuscation::kEnterpriseFileObfuscation);

  int length = strlen(kTestData1) + strlen(kTestData2) + strlen(kTestData3);
  ASSERT_TRUE(CreateDownloadFile(length, false));
  const char* chunks[] = {kTestData1, kTestData2, kTestData3};

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();

  AppendDataToFile(chunks, 3);
  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kDataHash);

  // Verify that the file content is not obfuscated.
  std::string file_content;
  ASSERT_TRUE(
      base::ReadFileToString(download_file_->FullPath(), &file_content));
  EXPECT_EQ(file_content, std::string(kTestData1) + std::string(kTestData2) +
                              std::string(kTestData3));

  DestroyDownloadFile(0);
}

TEST_F(DownloadFileTestWithObfuscation, DeobfuscateAndRename) {
  size_t length = strlen(kTestData1) + strlen(kTestData2) + strlen(kTestData3);
  ASSERT_TRUE(CreateDownloadFile(length, true));
  const char* chunks[] = {kTestData1, kTestData2, kTestData3};

  EXPECT_CALL(*input_stream_, RegisterDataReadyCallback(_))
      .Times(1)
      .RetiresOnSaturation();

  expected_data_ += std::string(CalculateObfuscationOverhead(3), '\0');
  AppendDataToFile(chunks, 3);
  FinishStream(DOWNLOAD_INTERRUPT_REASON_NONE, true, kDataHash);

  // Deobfuscate the file in place.
  base::expected<void, enterprise_obfuscation::Error> deobfuscate_result =
      enterprise_obfuscation::DeobfuscateFileInPlace(
          download_file_->FullPath());
  EXPECT_TRUE(deobfuscate_result.has_value());

  EXPECT_CALL(quarantine_, QuarantineFile(_, _, _, _, _, _))
      .WillOnce(WithArg<5>(
          [](quarantine::mojom::Quarantine::QuarantineFileCallback callback) {
            std::move(callback).Run(
                quarantine::mojom::QuarantineFileResult::OK);
          }));

  // Test renaming after deobfuscation.
  base::FilePath initial_path(download_file_->FullPath());
  base::FilePath new_path(initial_path.InsertBeforeExtensionASCII("_renamed"));
  DownloadInterruptReason rename_reason = RenameAndAnnotate(new_path, nullptr);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, rename_reason);
  EXPECT_TRUE(base::PathExists(new_path));
  EXPECT_FALSE(base::PathExists(initial_path));

  // Verify the final file size after renaming.
  int64_t final_size;
  ASSERT_TRUE(base::GetFileSize(new_path, &final_size));
  EXPECT_EQ(length, static_cast<size_t>(final_size));

  DestroyDownloadFile(0, false);
}
#endif

}  // namespace download
