// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/download/public/common/download_item_impl.h"

#include <stdint.h>

#include <iterator>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_impl_delegate.h"
#include "components/download/public/common/download_target_info.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/download/public/common/mock_download_file.h"
#include "crypto/secure_hash.h"
#include "net/http/http_response_headers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::ReturnRefOfCopy;
using ::testing::SaveArg;
using ::testing::StrictMock;
using ::testing::WithArg;

const int kDownloadChunkSize = 1000;
const int kDownloadSpeed = 1000;
const base::FilePath::CharType kDummyTargetPath[] =
    FILE_PATH_LITERAL("/testpath");
const base::FilePath::CharType kDummyIntermediatePath[] =
    FILE_PATH_LITERAL("/testpathx");

namespace download {
namespace {

template <typename T>
base::HistogramBase::Sample ToHistogramSample(T t) {
  return static_cast<base::HistogramBase::Sample>(t);
}

class MockDelegate : public DownloadItemImplDelegate {
 public:
  MockDelegate() { SetDefaultExpectations(); }

  void DetermineDownloadTarget(DownloadItemImpl* item,
                               download::DownloadTargetCallback cb) override {
    DetermineDownloadTarget_(item, cb);
  }
  MOCK_METHOD2(DetermineDownloadTarget_,
               void(DownloadItemImpl*, download::DownloadTargetCallback&));
  bool ShouldCompleteDownload(DownloadItemImpl* item,
                              base::OnceClosure cb) override {
    return ShouldCompleteDownload_(item, cb);
  }
  MOCK_METHOD2(ShouldCompleteDownload_,
               bool(DownloadItemImpl*, base::OnceClosure&));
  bool ShouldOpenDownload(DownloadItemImpl* item,
                          ShouldOpenDownloadCallback cb) override {
    return ShouldOpenDownload_(item, cb);
  }
  MOCK_METHOD2(ShouldOpenDownload_,
               bool(DownloadItemImpl*, ShouldOpenDownloadCallback&));
  MOCK_METHOD1(ShouldOpenFileBasedOnExtension, bool(const base::FilePath&));
  MOCK_METHOD1(CheckForFileRemoval, void(DownloadItemImpl*));

  void ResumeInterruptedDownload(
      std::unique_ptr<DownloadUrlParameters> params,
      const std::string& serialized_embedder_download_data) override {
    MockResumeInterruptedDownload(params.get());
  }
  MOCK_METHOD1(MockResumeInterruptedDownload,
               void(DownloadUrlParameters* params));

  MOCK_METHOD1(DownloadOpened, void(DownloadItemImpl*));
  MOCK_METHOD1(DownloadRemoved, void(DownloadItemImpl*));
  MOCK_CONST_METHOD0(IsOffTheRecord, bool());
  MOCK_METHOD(bool, IsActiveNetworkMetered, (), (const override));

  void VerifyAndClearExpectations() {
    ::testing::Mock::VerifyAndClearExpectations(this);
    SetDefaultExpectations();
  }

 private:
  void SetDefaultExpectations() {
    EXPECT_CALL(*this, ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*this, ShouldOpenDownload_(_, _)).WillRepeatedly(Return(true));
    EXPECT_CALL(*this, IsOffTheRecord()).WillRepeatedly(Return(false));
    EXPECT_CALL(*this, IsActiveNetworkMetered).WillRepeatedly(Return(false));
  }
};

class TestDownloadItemObserver : public DownloadItem::Observer {
 public:
  explicit TestDownloadItemObserver(DownloadItem* item)
      : item_(item),
        last_state_(item->GetState()),
        removed_(false),
        destroyed_(false),
        updated_(false),
        interrupt_count_(0),
        resume_count_(0) {
    item_->AddObserver(this);
  }

  ~TestDownloadItemObserver() override {
    if (item_)
      item_->RemoveObserver(this);
  }

  bool download_removed() const { return removed_; }
  bool download_destroyed() const { return destroyed_; }
  int interrupt_count() const { return interrupt_count_; }
  int resume_count() const { return resume_count_; }

  bool CheckAndResetDownloadUpdated() {
    bool was_updated = updated_;
    updated_ = false;
    return was_updated;
  }

 private:
  void OnDownloadRemoved(DownloadItem* download) override {
    SCOPED_TRACE(::testing::Message() << " " << __FUNCTION__ << " download = "
                                      << download->DebugString(false));
    removed_ = true;
  }

  void OnDownloadUpdated(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
    updated_ = true;
    DownloadItem::DownloadState new_state = download->GetState();
    if (last_state_ == DownloadItem::IN_PROGRESS &&
        new_state == DownloadItem::INTERRUPTED) {
      interrupt_count_++;
    }
    if (last_state_ == DownloadItem::INTERRUPTED &&
        new_state == DownloadItem::IN_PROGRESS) {
      resume_count_++;
    }
    last_state_ = new_state;
  }

  void OnDownloadOpened(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
  }

  void OnDownloadDestroyed(DownloadItem* download) override {
    DVLOG(20) << " " << __FUNCTION__
              << " download = " << download->DebugString(false);
    destroyed_ = true;
    item_->RemoveObserver(this);
    item_ = nullptr;
  }

  raw_ptr<DownloadItem> item_;
  DownloadItem::DownloadState last_state_;
  bool removed_;
  bool destroyed_;
  bool updated_;
  int interrupt_count_;
  int resume_count_;
};

// Schedules a task to invoke a callback that's bound to the specified
// parameter.
// E.g.:
//
//   EXPECT_CALL(foo, Bar(1, _))
//     .WithArg<1>(ScheduleCallbackWithParams(0, 0, task_runner));
//
//   .. will invoke the second argument to Bar with 0 as the parameter.
ACTION_P3(ScheduleCallbackWithParams, param1, param2, task_runner) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(arg0), param1, param2));
}

const char kTestData1[] = {'M', 'a', 'r', 'y', ' ', 'h', 'a', 'd',
                           ' ', 'a', ' ', 'l', 'i', 't', 't', 'l',
                           'e', ' ', 'l', 'a', 'm', 'b', '.'};

// SHA256 hash of TestData1
const uint8_t kHashOfTestData1[] = {
    0xd2, 0xfc, 0x16, 0xa1, 0xf5, 0x1a, 0x65, 0x3a, 0xa0, 0x19, 0x64,
    0xef, 0x9c, 0x92, 0x33, 0x36, 0xe1, 0x06, 0x53, 0xfe, 0xc1, 0x95,
    0xf4, 0x93, 0x45, 0x8b, 0x3b, 0x21, 0x89, 0x0e, 0x1b, 0x97};

class DownloadItemTest : public testing::Test {
 public:
  DownloadItemTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED),
        next_download_id_(DownloadItem::kInvalidId + 1) {
    create_info_ = std::make_unique<DownloadCreateInfo>();
    create_info_->save_info = std::make_unique<DownloadSaveInfo>();
    create_info_->save_info->prompt_for_save_location = false;
    create_info_->url_chain.push_back(GURL("http://example.com/download"));
    create_info_->etag = "SomethingToSatisfyResumption";
    create_info_->request_initiator =
        url::Origin::Create(GURL("http://example.com"));
  }

  DownloadItemImpl* CreateDownloadItemWithCreateInfo(
      std::unique_ptr<DownloadCreateInfo> info) {
    DownloadItemImpl* download = new DownloadItemImpl(
        mock_delegate(), next_download_id_++, *(info.get()));
    allocated_downloads_[download] = base::WrapUnique(download);
    return download;
  }

  // Creates a new net::HttpResponseHeaders object for the |response_code|.
  scoped_refptr<const net::HttpResponseHeaders> CreateResponseHeaders(
      int response_code) {
    return base::MakeRefCounted<net::HttpResponseHeaders>(
        "HTTP/1.1 " + base::NumberToString(response_code));
  }

  // This class keeps ownership of the created download item; it will
  // be torn down at the end of the test unless DestroyDownloadItem is
  // called.
  DownloadItemImpl* CreateDownloadItem() {
    DownloadItemImpl* download = new DownloadItemImpl(
        mock_delegate(), ++next_download_id_, *create_info_);
    allocated_downloads_[download] = base::WrapUnique(download);
    return download;
  }

  // Add DownloadFile to DownloadItem.
  MockDownloadFile* CallDownloadItemStart(
      DownloadItemImpl* item,
      download::DownloadTargetCallback* callback) {
    MockDownloadFile* mock_download_file = nullptr;
    std::unique_ptr<DownloadFile> download_file;
    EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(item, _))
        .WillOnce(MoveArg<1>(callback));

    // Only create a DownloadFile if the request was successful.
    if (create_info_->result == DOWNLOAD_INTERRUPT_REASON_NONE) {
      mock_download_file = new StrictMock<MockDownloadFile>;
      download_file.reset(mock_download_file);
      EXPECT_CALL(*mock_download_file, Initialize(_, _, _))
          .WillOnce(ScheduleCallbackWithParams(
              DOWNLOAD_INTERRUPT_REASON_NONE, 0,
              base::SingleThreadTaskRunner::GetCurrentDefault()));
      EXPECT_CALL(*mock_download_file, FullPath())
          .WillRepeatedly(ReturnRefOfCopy(base::FilePath()));
    }

    item->Start(std::move(download_file), base::DoNothing(), *create_info_,
                URLLoaderFactoryProvider::GetNullPtr());
    task_environment_.RunUntilIdle();

    // So that we don't have a function writing to a stack variable
    // lying around if the above failed.
    mock_delegate()->VerifyAndClearExpectations();
    EXPECT_CALL(*mock_delegate(), ShouldOpenFileBasedOnExtension(_))
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_delegate(), ShouldOpenDownload_(_, _))
        .WillRepeatedly(Return(true));

    return mock_download_file;
  }

  // Perform the intermediate rename for |item|. The target path for the
  // download will be set to kDummyTargetPath. Returns the MockDownloadFile*
  // that was added to the DownloadItem.
  MockDownloadFile* DoIntermediateRename(DownloadItemImpl* item,
                                         DownloadDangerType danger_type) {
    EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
    EXPECT_TRUE(item->GetTargetFilePath().empty());
    download::DownloadTargetCallback callback;
    MockDownloadFile* file = CallDownloadItemStart(item, &callback);
    DoRenameAndRunTargetCallback(item, file, std::move(callback), danger_type);
    return file;
  }

  void DoRenameAndRunTargetCallback(DownloadItemImpl* item,
                                    MockDownloadFile* download_file,
                                    download::DownloadTargetCallback callback,
                                    DownloadDangerType danger_type) {
    base::FilePath target_path(kDummyTargetPath);
    base::FilePath intermediate_path(kDummyIntermediatePath);
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    SetRenameExpectation(download_file, task_runner, intermediate_path,
                         DOWNLOAD_INTERRUPT_REASON_NONE);

    download::DownloadTargetInfo target_info;
    target_info.target_path = target_path;
    target_info.intermediate_path = intermediate_path;
    target_info.danger_type = danger_type;

    std::move(callback).Run(std::move(target_info));
    task_environment_.RunUntilIdle();
  }

  void SetRenameExpectation(
      MockDownloadFile* download_file,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      const base::FilePath& new_file_path,
      DownloadInterruptReason reason) {
    EXPECT_CALL(*download_file, RenameAndUniquify(_, _))
        .WillOnce(WithArg<1>([task_runner, new_file_path, reason](
                                 DownloadFile::RenameCompletionCallback cb) {
          task_runner->PostTask(
              FROM_HERE, base::BindOnce(std::move(cb), reason, new_file_path));
        }));
  }

  void DoDestinationComplete(DownloadItemImpl* item,
                             MockDownloadFile* download_file) {
    EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(_, _))
        .WillOnce(Return(true));
    base::FilePath final_path(kDummyTargetPath);
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, _, _, _, _, _))
        .WillOnce(WithArg<6>([&task_runner, &final_path](
                                 DownloadFile::RenameCompletionCallback cb) {
          task_runner->PostTask(
              FROM_HERE,
              base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                             final_path));
        }));
    EXPECT_CALL(*download_file, FullPath())
        .WillRepeatedly(ReturnRefOfCopy(base::FilePath(kDummyTargetPath)));
    EXPECT_CALL(*download_file, Detach());

    item->DestinationObserverAsWeakPtr()->DestinationCompleted(
        0, std::unique_ptr<crypto::SecureHash>());
    task_environment_.RunUntilIdle();
  }

  // Cleanup a download item (specifically get rid of the DownloadFile on it).
  // The item must be in the expected state.
  void CleanupItem(DownloadItemImpl* item,
                   MockDownloadFile* download_file,
                   DownloadItem::DownloadState expected_state) {
    EXPECT_EQ(expected_state, item->GetState());

    if (expected_state == DownloadItem::IN_PROGRESS) {
      if (download_file)
        EXPECT_CALL(*download_file, Cancel());
      item->Cancel(true);
      task_environment_.RunUntilIdle();
    }
  }

  // Destroy a previously created download item.
  void DestroyDownloadItem(DownloadItem* item) {
    allocated_downloads_.erase(item);
  }

  MockDelegate* mock_delegate() { return &mock_delegate_; }

  void OnDownloadFileAcquired(base::FilePath* return_path,
                              const base::FilePath& path) {
    *return_path = path;
  }

  DownloadCreateInfo* create_info() { return create_info_.get(); }

  void CancelRequest(bool user_cancel) { canceled_ = true; }

  bool canceled() { return canceled_; }

  base::test::TaskEnvironment task_environment_;

 private:
  StrictMock<MockDelegate> mock_delegate_;
  std::map<DownloadItem*, std::unique_ptr<DownloadItem>> allocated_downloads_;
  std::unique_ptr<DownloadCreateInfo> create_info_;
  uint32_t next_download_id_ = DownloadItem::kInvalidId + 1;
  bool canceled_ = false;
};

// Tests to ensure calls that change a DownloadItem generate an update
// to observers. State changing functions not tested:
//  void OpenDownload();
//  void ShowDownloadInShell();
//  void CompleteDelayedDownload();
//  set_* mutators

TEST_F(DownloadItemTest, NotificationAfterUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  TestDownloadItemObserver observer(item);

  item->DestinationUpdate(kDownloadChunkSize, kDownloadSpeed,
                          std::vector<DownloadItem::ReceivedSlice>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(kDownloadSpeed, item->CurrentSpeed());
  CleanupItem(item, file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterCancel) {
  DownloadItemImpl* user_cancel = CreateDownloadItem();
  download::DownloadTargetCallback target_callback;
  MockDownloadFile* download_file =
      CallDownloadItemStart(user_cancel, &target_callback);
  EXPECT_CALL(*download_file, Cancel());

  TestDownloadItemObserver observer1(user_cancel);
  user_cancel->Cancel(true);
  ASSERT_TRUE(observer1.CheckAndResetDownloadUpdated());

  DownloadItemImpl* system_cancel = CreateDownloadItem();
  download_file = CallDownloadItemStart(system_cancel, &target_callback);
  EXPECT_CALL(*download_file, Cancel());

  TestDownloadItemObserver observer2(system_cancel);
  system_cancel->Cancel(false);
  ASSERT_TRUE(observer2.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterComplete) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  DoDestinationComplete(item, download_file);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDownloadedFileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);

  item->OnDownloadedFileRemoved();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterInterrupted) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_CALL(*download_file, Cancel());
  TestDownloadItemObserver observer(item);

  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_)).Times(0);

  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, 0,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
}

TEST_F(DownloadItemTest, NotificationAfterDestroyed) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);

  DestroyDownloadItem(item);
  ASSERT_TRUE(observer.download_destroyed());
}

TEST_F(DownloadItemTest, NotificationAfterRemove) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback target_callback;
  MockDownloadFile* download_file =
      CallDownloadItemStart(item, &target_callback);
  EXPECT_CALL(*download_file, Cancel());
  EXPECT_CALL(*mock_delegate(), DownloadRemoved(_));
  TestDownloadItemObserver observer(item);

  item->Remove();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  ASSERT_TRUE(observer.download_removed());
}

TEST_F(DownloadItemTest, NotificationAfterOnContentCheckCompleted) {
  // Setting to NOT_DANGEROUS does not trigger a notification.
  DownloadItemImpl* safe_item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(safe_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver safe_observer(safe_item);

  safe_item->OnAllDataSaved(0, std::unique_ptr<crypto::SecureHash>());
  EXPECT_TRUE(safe_observer.CheckAndResetDownloadUpdated());
  safe_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
                                     DOWNLOAD_INTERRUPT_REASON_NONE);
  EXPECT_TRUE(safe_observer.CheckAndResetDownloadUpdated());
  CleanupItem(safe_item, download_file, DownloadItem::IN_PROGRESS);

  // Setting to unsafe url or unsafe file should trigger a notification.
  DownloadItemImpl* unsafeurl_item = CreateDownloadItem();
  download_file =
      DoIntermediateRename(unsafeurl_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver unsafeurl_observer(unsafeurl_item);

  unsafeurl_item->OnAllDataSaved(0, std::unique_ptr<crypto::SecureHash>());
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());
  unsafeurl_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_URL,
                                          DOWNLOAD_INTERRUPT_REASON_NONE);
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, _, _, _, _, _));
  unsafeurl_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafeurl_observer.CheckAndResetDownloadUpdated());
  CleanupItem(unsafeurl_item, download_file, DownloadItem::IN_PROGRESS);

  DownloadItemImpl* unsafefile_item = CreateDownloadItem();
  download_file =
      DoIntermediateRename(unsafefile_item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  TestDownloadItemObserver unsafefile_observer(unsafefile_item);

  unsafefile_item->OnAllDataSaved(0, std::unique_ptr<crypto::SecureHash>());
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());
  unsafefile_item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
                                           DOWNLOAD_INTERRUPT_REASON_NONE);
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(_, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, _, _, _, _, _));
  unsafefile_item->ValidateDangerousDownload();
  EXPECT_TRUE(unsafefile_observer.CheckAndResetDownloadUpdated());
  CleanupItem(unsafefile_item, download_file, DownloadItem::IN_PROGRESS);
}

// DownloadItemImpl::OnDownloadTargetDetermined will schedule a task to run
// DownloadFile::Rename(). Once the rename
// completes, DownloadItemImpl receives a notification with the new file
// name. Check that observers are updated when the new filename is available and
// not before.
TEST_F(DownloadItemTest, NotificationAfterOnDownloadTargetDetermined) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  TestDownloadItemObserver observer(item);
  base::FilePath target_path(kDummyTargetPath);
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      target_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);

  // Currently, a notification would be generated if the danger type is anything
  // other than NOT_DANGEROUS.
  download::DownloadTargetInfo target_info;
  target_info.target_path = target_path;
  target_info.intermediate_path = intermediate_path;

  std::move(callback).Run(std::move(target_info));
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, NotificationAfterTogglePause) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  std::unique_ptr<DownloadFile> download_file(mock_download_file);

  EXPECT_CALL(*mock_download_file, Initialize(_, _, _));
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(_, _));
  item->Start(std::move(download_file), base::DoNothing(), *create_info(),
              URLLoaderFactoryProvider::GetNullPtr());

  item->Pause();
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());

  ASSERT_TRUE(item->IsPaused());

  item->Resume(false);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());

  task_environment_.RunUntilIdle();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

// Test that a download is resumed automatically after a continuable interrupt.
TEST_F(DownloadItemTest, AutomaticResumption_Continue) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download using a continuable interrupt after writing a single
  // byte. An intermediate file with data shouldn't be discarding after a
  // continuable interrupt.

  // The DownloadFile should be detached without discarding.
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());

  // Resumption attempt should pass the intermediate file along.
  EXPECT_CALL(
      *mock_delegate(),
      MockResumeInterruptedDownload(AllOf(
          Property(&DownloadUrlParameters::file_path,
                   Property(&base::FilePath::value, kDummyIntermediatePath)),
          Property(&DownloadUrlParameters::offset, 1))));

  base::HistogramTester histogram_tester;
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 1,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Since the download is resumed automatically, the interrupt count doesn't
  // increase.
  ASSERT_EQ(0, observer.interrupt_count());

  // Test expectations verify that ResumeInterruptedDownload() is called (by way
  // of MockResumeInterruptedDownload) after the download is interrupted. But
  // the mock doesn't follow through with the resumption.
  // ResumeInterruptedDownload() being called is sufficient for verifying that
  // the automatic resumption was triggered.
  task_environment_.RunUntilIdle();

  // Interrupt reason is recorded in auto resumption even when download is not
  // finally interrupted.
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR),
      1);

  // The download item is currently in RESUMING_INTERNAL state, which maps to
  // IN_PROGRESS.
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

// Automatic resumption should restart and discard the intermediate file if the
// interrupt reason requires it.
TEST_F(DownloadItemTest, AutomaticResumption_Restart) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a restartable interrupt.
  EXPECT_CALL(*download_file, Cancel());
  EXPECT_EQ(kDummyIntermediatePath, item->GetFullPath().value());

  // Resumption attempt should have discarded intermediate file.
  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(Property(
                                    &DownloadUrlParameters::file_path,
                                    Property(&base::FilePath::empty, true))));

  base::HistogramTester histogram_tester;
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE, 1,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());

  // Since the download is resumed automatically, the interrupt count doesn't
  // increase.
  ASSERT_EQ(0, observer.interrupt_count());

  task_environment_.RunUntilIdle();
  // Auto resumption will record interrupt reason even if download is not
  // finally interrupted.
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE),
      1);
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

// Test that automatic resumption doesn't happen after an interrupt that
// requires user action to resolve.
TEST_F(DownloadItemTest, AutomaticResumption_NeedsUserAction) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a restartable interrupt.
  EXPECT_CALL(*download_file, Cancel());
  base::HistogramTester histogram_tester;
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, 1,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.interrupt_count());
  ASSERT_EQ(0, observer.resume_count());

  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount("Download.InterruptedReason",
                                     ToHistogramSample<DownloadInterruptReason>(
                                         DOWNLOAD_INTERRUPT_REASON_FILE_FAILED),
                                     1);
  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

// Test that a download is resumed automatically after a content length mismatch
// error.
TEST_F(DownloadItemTest, AutomaticResumption_ContentLengthMismatch) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download with content length mismatch error. The intermediate
  // file with data shouldn't be discarded.

  // The DownloadFile should be detached without discarding.
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());

  // Resumption attempt should pass the intermediate file along.
  EXPECT_CALL(
      *mock_delegate(),
      MockResumeInterruptedDownload(AllOf(
          Property(&DownloadUrlParameters::file_path,
                   Property(&base::FilePath::value, kDummyIntermediatePath)),
          Property(&DownloadUrlParameters::offset, 1))));

  base::HistogramTester histogram_tester;
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH, 1,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Since the download is resumed automatically, the observer shouldn't notice
  // the interruption.
  ASSERT_EQ(0, observer.interrupt_count());
  ASSERT_EQ(0, observer.resume_count());

  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_SERVER_CONTENT_LENGTH_MISMATCH),
      1);
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

// Check we do correct cleanup for RESUME_MODE_INVALID interrupts.
TEST_F(DownloadItemTest, UnresumableInterrupt) {
  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Fail final rename with unresumable reason.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(base::FilePath(kDummyTargetPath), _, _, _, _, _, _))
      .WillOnce(WithArg<6>([&task_runner](
                               DownloadFile::RenameCompletionCallback cb) {
        task_runner->PostTask(
            FROM_HERE, base::BindOnce(std::move(cb),
                                      DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED,
                                      base::FilePath()));
      }));
  EXPECT_CALL(*download_file, Cancel());

  // Complete download to trigger final rename.
  base::HistogramTester histogram_tester;
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());

  task_environment_.RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_FILE_BLOCKED),
      1);
  ASSERT_TRUE(observer.CheckAndResetDownloadUpdated());
  // Should not try to auto-resume.
  ASSERT_EQ(1, observer.interrupt_count());
  ASSERT_EQ(0, observer.resume_count());

  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

TEST_F(DownloadItemTest, AutomaticResumption_AttemptLimit) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);
  MockDownloadFile* mock_download_file_ref = nullptr;
  std::unique_ptr<MockDownloadFile> mock_download_file;
  download::DownloadTargetCallback callback;

  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(item, _))
      .WillRepeatedly(MoveArg<1>(&callback));

  // All attempts at resumption should pass along the intermediate file.
  EXPECT_CALL(
      *mock_delegate(),
      MockResumeInterruptedDownload(AllOf(
          Property(&DownloadUrlParameters::file_path,
                   Property(&base::FilePath::value, kDummyIntermediatePath)),
          Property(&DownloadUrlParameters::offset, 1))))
      .Times(DownloadItemImpl::kMaxAutoResumeAttempts);
  for (int i = 0; i < (DownloadItemImpl::kMaxAutoResumeAttempts + 1); ++i) {
    SCOPED_TRACE(::testing::Message() << "Iteration " << i);

    mock_download_file = std::make_unique<NiceMock<MockDownloadFile>>();
    mock_download_file_ref = mock_download_file.get();

    ON_CALL(*mock_download_file_ref, FullPath())
        .WillByDefault(ReturnRefOfCopy(base::FilePath()));

    // Copied key parts of DoIntermediateRename & CallDownloadItemStart
    // to allow for holding onto the request handle.
    item->Start(std::move(mock_download_file), base::DoNothing(),
                *create_info(), URLLoaderFactoryProvider::GetNullPtr());
    task_environment_.RunUntilIdle();

    base::FilePath target_path(kDummyTargetPath);
    base::FilePath intermediate_path(kDummyIntermediatePath);
    auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
    // RenameAndUniquify is only called the first time. In all the subsequent
    // iterations, the intermediate file already has the correct name, hence
    // no rename is necessary.
    if (i == 0) {
      SetRenameExpectation(mock_download_file_ref, task_runner,
                           intermediate_path, DOWNLOAD_INTERRUPT_REASON_NONE);
    }
    ASSERT_TRUE(callback);

    download::DownloadTargetInfo target_info;
    target_info.target_path = target_path;
    target_info.intermediate_path = intermediate_path;

    std::move(callback).Run(std::move(target_info));
    task_environment_.RunUntilIdle();

    // Use a continuable interrupt.
    EXPECT_CALL(*mock_download_file_ref, Cancel()).Times(0);
    item->DestinationObserverAsWeakPtr()->DestinationError(
        DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 1,
        std::unique_ptr<crypto::SecureHash>());

    task_environment_.RunUntilIdle();
    ::testing::Mock::VerifyAndClearExpectations(mock_download_file_ref);
  }

  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR),
      DownloadItemImpl::kMaxAutoResumeAttempts + 1);
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(1, observer.interrupt_count());
  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

// If the download attempts to resume and the resumption request fails, the
// subsequent Start() call shouldn't update the origin state (URL redirect
// chains, Content-Disposition, download URL, etc..)
TEST_F(DownloadItemTest, FailedResumptionDoesntUpdateOriginState) {
  constexpr int kFirstResponseCode = 200;
  const char kContentDisposition[] = "attachment; filename=foo";
  const char kFirstETag[] = "ABC";
  const char kFirstLastModified[] = "Yesterday";
  const char kFirstURL[] = "http://www.example.com/download";
  const char kMimeType[] = "text/css";
  create_info()->response_headers = CreateResponseHeaders(kFirstResponseCode);
  create_info()->content_disposition = kContentDisposition;
  create_info()->etag = kFirstETag;
  create_info()->last_modified = kFirstLastModified;
  create_info()->url_chain.push_back(GURL(kFirstURL));
  create_info()->mime_type = kMimeType;

  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  ASSERT_TRUE(item->GetResponseHeaders());
  EXPECT_EQ(kFirstResponseCode, item->GetResponseHeaders()->response_code());
  EXPECT_EQ(kContentDisposition, item->GetContentDisposition());
  EXPECT_EQ(kFirstETag, item->GetETag());
  EXPECT_EQ(kFirstLastModified, item->GetLastModifiedTime());
  EXPECT_EQ(kFirstURL, item->GetURL().spec());
  EXPECT_EQ(kMimeType, item->GetMimeType());

  EXPECT_CALL(
      *mock_delegate(),
      MockResumeInterruptedDownload(AllOf(
          Property(&DownloadUrlParameters::file_path,
                   Property(&base::FilePath::value, kDummyIntermediatePath)),
          Property(&DownloadUrlParameters::offset, 1))));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 1,
      std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Now change the create info. The changes should not cause the
  // DownloadItem to be updated.
  constexpr int kSecondResponseCode = 418;
  const char kSecondContentDisposition[] = "attachment; filename=bar";
  const char kSecondETag[] = "123";
  const char kSecondLastModified[] = "Today";
  const char kSecondURL[] = "http://example.com/another-download";
  const char kSecondMimeType[] = "text/html";
  create_info()->response_headers = CreateResponseHeaders(kSecondResponseCode);
  create_info()->content_disposition = kSecondContentDisposition;
  create_info()->etag = kSecondETag;
  create_info()->last_modified = kSecondLastModified;
  create_info()->url_chain.clear();
  create_info()->url_chain.push_back(GURL(kSecondURL));
  create_info()->mime_type = kSecondMimeType;
  create_info()->result = DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
  create_info()->save_info->file_path = base::FilePath(kDummyIntermediatePath);
  create_info()->save_info->offset = 1;

  // Calling Start() with a response indicating failure shouldn't cause a target
  // update, nor should it result in discarding the intermediate file.
  download::DownloadTargetCallback target_callback;
  download_file = CallDownloadItemStart(item, &target_callback);
  ASSERT_TRUE(target_callback);

  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(kDummyTargetPath);
  target_info.intermediate_path = base::FilePath(kDummyIntermediatePath);
  std::move(target_callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(item->GetResponseHeaders());
  EXPECT_EQ(kFirstResponseCode, item->GetResponseHeaders()->response_code());
  EXPECT_EQ(kContentDisposition, item->GetContentDisposition());
  EXPECT_EQ(kFirstETag, item->GetETag());
  EXPECT_EQ(kFirstLastModified, item->GetLastModifiedTime());
  EXPECT_EQ(kFirstURL, item->GetURL().spec());
  EXPECT_EQ(kMimeType, item->GetMimeType());
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item->GetLastReason());
  EXPECT_EQ(kDummyIntermediatePath, item->GetFullPath().value());
  EXPECT_EQ(1, item->GetReceivedBytes());
}

// If the download resumption request succeeds, the origin state should be
// updated.
TEST_F(DownloadItemTest, SucceededResumptionUpdatesOriginState) {
  constexpr int kFirstResponseCode = 200;
  const char kContentDisposition[] = "attachment; filename=foo";
  const char kFirstETag[] = "ABC";
  const char kFirstLastModified[] = "Yesterday";
  const char kFirstURL[] = "http://www.example.com/download";
  const char kMimeType[] = "text/css";
  create_info()->response_headers = CreateResponseHeaders(kFirstResponseCode);
  create_info()->content_disposition = kContentDisposition;
  create_info()->etag = kFirstETag;
  create_info()->last_modified = kFirstLastModified;
  create_info()->url_chain.push_back(GURL(kFirstURL));
  create_info()->mime_type = kMimeType;

  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 0,
      std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  // Now change the create info. The changes should not cause the
  // DownloadItem to be updated.
  constexpr int kSecondResponseCode = 201;
  const char kSecondContentDisposition[] = "attachment; filename=bar";
  const char kSecondETag[] = "123";
  const char kSecondLastModified[] = "Today";
  const char kSecondURL[] = "http://example.com/another-download";
  const char kSecondMimeType[] = "text/html";
  create_info()->response_headers = CreateResponseHeaders(kSecondResponseCode);
  create_info()->content_disposition = kSecondContentDisposition;
  create_info()->etag = kSecondETag;
  create_info()->last_modified = kSecondLastModified;
  create_info()->url_chain.clear();
  create_info()->url_chain.push_back(GURL(kSecondURL));
  create_info()->mime_type = kSecondMimeType;

  download::DownloadTargetCallback target_callback;
  download_file = CallDownloadItemStart(item, &target_callback);

  ASSERT_TRUE(item->GetResponseHeaders());
  EXPECT_EQ(kSecondResponseCode, item->GetResponseHeaders()->response_code());
  EXPECT_EQ(kSecondContentDisposition, item->GetContentDisposition());
  EXPECT_EQ(kSecondETag, item->GetETag());
  EXPECT_EQ(kSecondLastModified, item->GetLastModifiedTime());
  EXPECT_EQ(kSecondURL, item->GetURL().spec());
  EXPECT_EQ(kSecondMimeType, item->GetMimeType());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Ensure when strong validators changed on resumption, the received
// slices should be cleared.
TEST_F(DownloadItemTest, ClearReceivedSliceIfEtagChanged) {
  const char kFirstETag[] = "ABC";
  const char kSecondETag[] = "123";
  const DownloadItem::ReceivedSlices kReceivedSlice = {
      DownloadItem::ReceivedSlice(0, 10)};
  create_info()->etag = kFirstETag;

  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_));
  EXPECT_CALL(*download_file, Detach());

  item->DestinationObserverAsWeakPtr()->DestinationUpdate(10, 100,
                                                          kReceivedSlice);
  EXPECT_EQ(kReceivedSlice, item->GetReceivedSlices());
  EXPECT_EQ(10, item->GetReceivedBytes());

  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 0,
      std::unique_ptr<crypto::SecureHash>());
  EXPECT_EQ(kReceivedSlice, item->GetReceivedSlices());

  task_environment_.RunUntilIdle();

  // Change the strong validator and resume the download, the received slices
  // should be cleared.
  create_info()->etag = kSecondETag;
  download::DownloadTargetCallback target_callback;
  download_file = CallDownloadItemStart(item, &target_callback);
  EXPECT_TRUE(item->GetReceivedSlices().empty());
  EXPECT_EQ(0, item->GetReceivedBytes());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Ensure when a network socket error happens on resumption, the received slices
// info should be kept if the download is not restarted from beginning, so the
// download progress will not move backward.
TEST_F(DownloadItemTest, KeepReceivedSliceIfNetworkError) {
  const char kFirstETag[] = "ABC";
  const DownloadItem::ReceivedSlices kReceivedSlice = {
      DownloadItem::ReceivedSlice(0, 10), DownloadItem::ReceivedSlice(20, 30)};
  create_info()->etag = kFirstETag;

  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*mock_delegate(), MockResumeInterruptedDownload(_));
  EXPECT_CALL(*download_file, Detach());

  item->DestinationObserverAsWeakPtr()->DestinationUpdate(20, 100,
                                                          kReceivedSlice);
  EXPECT_EQ(kReceivedSlice, item->GetReceivedSlices());
  EXPECT_EQ(20, item->GetReceivedBytes());

  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 20 /* bytes_so_far */,
      std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();

  // Simulate a socket error, and start the download.
  create_info()->result = DOWNLOAD_INTERRUPT_REASON_NETWORK_TIMEOUT;
  download::DownloadTargetCallback target_callback;
  download_file = CallDownloadItemStart(item, &target_callback);

  // After starting the download, the slice info and received bytes should not
  // change.
  EXPECT_EQ(kReceivedSlice, item->GetReceivedSlices());
  EXPECT_EQ(20, item->GetReceivedBytes());

  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Test that resumption uses the final URL in a URL chain when resuming.
TEST_F(DownloadItemTest, ResumeUsesFinalURL) {
  create_info()->save_info->prompt_for_save_location = false;
  create_info()->url_chain.clear();
  create_info()->url_chain.push_back(GURL("http://example.com/a"));
  create_info()->url_chain.push_back(GURL("http://example.com/b"));
  create_info()->url_chain.push_back(GURL("http://example.com/c"));

  DownloadItemImpl* item = CreateDownloadItem();
  TestDownloadItemObserver observer(item);
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Interrupt the download, using a continuable interrupt.
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  EXPECT_CALL(*mock_delegate(),
              MockResumeInterruptedDownload(Property(
                  &DownloadUrlParameters::url, GURL("http://example.com/c"))))
      .Times(1);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_TRANSIENT_ERROR, 1,
      std::unique_ptr<crypto::SecureHash>());

  // Test expectations verify that ResumeInterruptedDownload() is called (by way
  // of MockResumeInterruptedDownload) after the download is interrupted. But
  // the mock doesn't follow through with the resumption.
  // ResumeInterruptedDownload() being called is sufficient for verifying that
  // the resumption was triggered.
  task_environment_.RunUntilIdle();

  // The download is currently in RESUMING_INTERNAL, which maps to IN_PROGRESS.
  CleanupItem(item, nullptr, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, DisplayName) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath target_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  EXPECT_EQ(FILE_PATH_LITERAL(""), item->GetFileNameToReportUser().value());
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);
  download::DownloadTargetInfo target_info;
  target_info.target_path = target_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(FILE_PATH_LITERAL("foo.bar"),
            item->GetFileNameToReportUser().value());
  item->SetDisplayName(base::FilePath(FILE_PATH_LITERAL("new.name")));
  EXPECT_EQ(FILE_PATH_LITERAL("new.name"),
            item->GetFileNameToReportUser().value());
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Test to make sure that Start method calls DF initialize properly.
TEST_F(DownloadItemTest, Start) {
  MockDownloadFile* mock_download_file(new MockDownloadFile);
  std::unique_ptr<DownloadFile> download_file(mock_download_file);
  DownloadItemImpl* item = CreateDownloadItem();
  EXPECT_CALL(*mock_download_file, Initialize(_, _, _));
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(item, _));
  item->Start(std::move(download_file), base::DoNothing(), *create_info(),
              URLLoaderFactoryProvider::GetNullPtr());
  task_environment_.RunUntilIdle();

  CleanupItem(item, mock_download_file, DownloadItem::IN_PROGRESS);
}

// Download file and the request should be cancelled as a result of download
// file initialization failing.
TEST_F(DownloadItemTest, InitDownloadFileFails) {
  DownloadItemImpl* item = CreateDownloadItem();
  std::unique_ptr<MockDownloadFile> file = std::make_unique<MockDownloadFile>();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(*file, Cancel());
  EXPECT_CALL(*file, Initialize(_, _, _))
      .WillOnce(ScheduleCallbackWithParams(
          DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED, 0,
          base::SingleThreadTaskRunner::GetCurrentDefault()));

  download::DownloadTargetCallback download_target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(item, _))
      .WillOnce(MoveArg<1>(&download_target_callback));

  item->Start(
      std::move(file),
      base::BindOnce(&DownloadItemTest::CancelRequest, base::Unretained(this)),
      *create_info(), URLLoaderFactoryProvider::GetNullPtr());
  task_environment_.RunUntilIdle();

  ASSERT_TRUE(download_target_callback);
  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(kDummyTargetPath);
  target_info.intermediate_path = base::FilePath(kDummyIntermediatePath);
  std::move(download_target_callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED,
            item->GetLastReason());
  EXPECT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_TRUE(canceled());
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED),
      1);
}

// Handling of downloads initiated via a failed request. In this case, Start()
// will get called with a DownloadCreateInfo with a non-zero interrupt_reason.
TEST_F(DownloadItemTest, StartFailedDownload) {
  base::HistogramTester histogram_tester;
  create_info()->result = DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED;
  DownloadItemImpl* item = CreateDownloadItem();

  // DownloadFile and DownloadRequestHandleInterface objects aren't created for
  // failed downloads.
  std::unique_ptr<DownloadFile> null_download_file;
  download::DownloadTargetCallback download_target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(item, _))
      .WillOnce(MoveArg<1>(&download_target_callback));
  item->Start(std::move(null_download_file), base::DoNothing(), *create_info(),
              URLLoaderFactoryProvider::GetNullPtr());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  task_environment_.RunUntilIdle();

  // The DownloadItemImpl should attempt to determine a target path even if the
  // download was interrupted.
  ASSERT_TRUE(download_target_callback);
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  base::FilePath target_path(FILE_PATH_LITERAL("foo"));
  download::DownloadTargetInfo target_info;
  target_info.target_path = target_path;
  target_info.intermediate_path = target_path;
  std::move(download_target_callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();

  // Interrupt reason carried in create info should be recorded.
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED),
      1);
  EXPECT_EQ(target_path, item->GetTargetFilePath());
  CleanupItem(item, nullptr, DownloadItem::INTERRUPTED);
}

// Test that the delegate is invoked after the download file is renamed.
TEST_F(DownloadItemTest, CallbackAfterRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);

  download::DownloadTargetInfo target_info;
  target_info.target_path = final_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, RenameAndAnnotate(final_path, _, _, _, _, _, _))
      .WillOnce(WithArg<6>([&task_runner, &final_path](
                               DownloadFile::RenameCompletionCallback cb) {
        task_runner->PostTask(
            FROM_HERE,
            base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                           final_path));
      }));

  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
}

// Test that the delegate is invoked after the download file is renamed and the
// download item is in an interrupted state.
TEST_F(DownloadItemTest, CallbackAfterInterruptedRename) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  base::HistogramTester histogram_tester;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  EXPECT_CALL(*download_file, Cancel()).Times(1);

  download::DownloadTargetInfo target_info;
  target_info.target_path = final_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  histogram_tester.ExpectBucketCount("Download.InterruptedReason",
                                     ToHistogramSample<DownloadInterruptReason>(
                                         DOWNLOAD_INTERRUPT_REASON_FILE_FAILED),
                                     1);
}

TEST_F(DownloadItemTest, Interrupted) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  const DownloadInterruptReason reason(
      DOWNLOAD_INTERRUPT_REASON_FILE_ACCESS_DENIED);

  // Confirm interrupt sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      reason, 0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(reason, item->GetLastReason());

  // Cancel should kill it.
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_USER_CANCELED, item->GetLastReason());

  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(reason), 1);
}

// Destination errors that occur before the intermediate rename shouldn't cause
// the download to be marked as interrupted until after the intermediate rename.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Restart) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, 0,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);

  EXPECT_CALL(*download_file, Cancel()).Times(1);

  download::DownloadTargetInfo target_info;
  target_info.target_path = final_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
  histogram_tester.ExpectBucketCount("Download.InterruptedReason",
                                     ToHistogramSample<DownloadInterruptReason>(
                                         DOWNLOAD_INTERRUPT_REASON_FILE_FAILED),
                                     1);
}

// As above. But if the download can be resumed by continuing, then the
// intermediate path should be retained when the download is interrupted after
// the intermediate rename succeeds.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Continue) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);

  // Write some data and interrupt with NETWORK_FAILED. The download shouldn't
  // transition to INTERRUPTED until the destination callback has been invoked.
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, 1,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath(new_intermediate_path)));
  EXPECT_CALL(*download_file, Detach());

  download::DownloadTargetInfo target_info;
  target_info.target_path = final_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(new_intermediate_path, item->GetFullPath());
  EXPECT_EQ(final_path, item->GetTargetFilePath());
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED),
      1);
}

// As above. If the intermediate rename fails, then the interrupt reason should
// be set to the file error and the intermediate path should be empty.
TEST_F(DownloadItemTest, InterruptedBeforeIntermediateRename_Failed) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, 0,
      std::unique_ptr<crypto::SecureHash>());
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());

  base::FilePath final_path(
      base::FilePath(kDummyTargetPath).AppendASCII("foo.bar"));
  base::FilePath intermediate_path(final_path.InsertBeforeExtensionASCII("x"));
  base::FilePath new_intermediate_path(
      final_path.InsertBeforeExtensionASCII("y"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(download_file, task_runner, new_intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_FILE_FAILED);
  EXPECT_CALL(*download_file, Cancel()).Times(1);

  download::DownloadTargetInfo target_info;
  target_info.target_path = final_path;
  target_info.intermediate_path = intermediate_path;
  std::move(callback).Run(std::move(target_info));
  task_environment_.RunUntilIdle();
  // All the callbacks should have happened by now.
  ::testing::Mock::VerifyAndClearExpectations(download_file);
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, item->GetLastReason());
  EXPECT_TRUE(item->GetFullPath().empty());
  EXPECT_EQ(final_path, item->GetTargetFilePath());

  // Rename error will overwrite the previous network interrupt reason.
  // TODO(xingliu): See if we should report both interrupted reasons or the
  // first one, see https://crbug.com/769040.
  histogram_tester.ExpectBucketCount("Download.InterruptedReason",
                                     ToHistogramSample<DownloadInterruptReason>(
                                         DOWNLOAD_INTERRUPT_REASON_FILE_FAILED),
                                     1);
  histogram_tester.ExpectTotalCount("Download.InterruptedReason", 1);
}

TEST_F(DownloadItemTest, Canceled) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback target_callback;
  MockDownloadFile* download_file =
      CallDownloadItemStart(item, &target_callback);

  // Confirm cancel sets state properly.
  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
}

TEST_F(DownloadItemTest, DownloadTargetDetermined_Cancel) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);

  EXPECT_CALL(*download_file, Cancel());
  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(FILE_PATH_LITERAL("foo"));
  target_info.intermediate_path = base::FilePath(FILE_PATH_LITERAL("bar"));
  target_info.interrupt_reason = DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;
  std::move(callback).Run(std::move(target_info));
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
}

TEST_F(DownloadItemTest, DownloadTargetDetermined_CancelWithEmptyName) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);

  EXPECT_CALL(*download_file, Cancel());
  std::move(callback).Run(download::DownloadTargetInfo());
  EXPECT_EQ(DownloadItem::CANCELLED, item->GetState());
}

TEST_F(DownloadItemTest, DownloadTargetDetermined_Conflict) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* download_file = CallDownloadItemStart(item, &callback);
  base::FilePath target_path(FILE_PATH_LITERAL("/foo/bar"));

  EXPECT_CALL(*download_file, Cancel());
  download::DownloadTargetInfo target_info;
  target_info.target_path = target_path;
  target_info.intermediate_path = target_path;
  target_info.interrupt_reason = DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE;
  std::move(callback).Run(std::move(target_info));
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_SAME_AS_SOURCE,
            item->GetLastReason());
}

TEST_F(DownloadItemTest, DownloadTargetDetermined_NewMimeType) {
  DownloadItemImpl* item = CreateDownloadItem();
  download::DownloadTargetCallback callback;
  MockDownloadFile* file = CallDownloadItemStart(item, &callback);
  base::FilePath target_path(FILE_PATH_LITERAL("/foo/bar"));
  base::FilePath intermediate_path(target_path.InsertBeforeExtensionASCII("x"));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  SetRenameExpectation(file, task_runner, intermediate_path,
                       DOWNLOAD_INTERRUPT_REASON_NONE);

  std::string mime_type = "application/pdf";
  download::DownloadTargetInfo target_info;
  target_info.target_path = target_path;
  target_info.intermediate_path = intermediate_path;
  target_info.mime_type = mime_type;
  std::move(callback).Run(std::move(target_info));
  EXPECT_EQ(mime_type, item->GetMimeType());
  CleanupItem(item, file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, FileRemoved) {
  DownloadItemImpl* item = CreateDownloadItem();

  EXPECT_FALSE(item->GetFileExternallyRemoved());
  item->OnDownloadedFileRemoved();
  EXPECT_TRUE(item->GetFileExternallyRemoved());
}

TEST_F(DownloadItemTest, DestinationUpdate) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(0l, item->CurrentSpeed());
  EXPECT_EQ(0l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());
  item->SetTotalBytes(100l);
  EXPECT_EQ(100l, item->GetTotalBytes());

  std::vector<DownloadItem::ReceivedSlice> received_slices;
  received_slices.emplace_back(0, 10);
  as_observer->DestinationUpdate(10, 20, received_slices);
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ(10l, item->GetReceivedBytes());
  EXPECT_EQ(100l, item->GetTotalBytes());
  EXPECT_EQ(received_slices, item->GetReceivedSlices());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());

  received_slices.emplace_back(200, 100);
  as_observer->DestinationUpdate(200, 20, received_slices);
  EXPECT_EQ(20l, item->CurrentSpeed());
  EXPECT_EQ(200l, item->GetReceivedBytes());
  EXPECT_EQ(0l, item->GetTotalBytes());
  EXPECT_EQ(received_slices, item->GetReceivedSlices());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());

  CleanupItem(item, file, DownloadItem::IN_PROGRESS);
}

TEST_F(DownloadItemTest, DestinationError_NoRestartRequired) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, item->GetLastReason());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());

  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(kTestData1, sizeof(kTestData1));

  EXPECT_CALL(*download_file, Detach());
  as_observer->DestinationError(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, 1,
                                std::move(hash));
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item->GetLastReason());
  EXPECT_EQ(
      std::string(std::begin(kHashOfTestData1), std::end(kHashOfTestData1)),
      item->GetHash());
  histogram_tester.ExpectBucketCount(
      "Download.InterruptedReason",
      ToHistogramSample<DownloadInterruptReason>(
          DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED),
      1);
}

TEST_F(DownloadItemTest, DestinationError_RestartRequired) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE, item->GetLastReason());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());

  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(kTestData1, sizeof(kTestData1));

  EXPECT_CALL(*download_file, Cancel());
  as_observer->DestinationError(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, 1,
                                std::move(hash));
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, item->GetLastReason());
  EXPECT_EQ(std::string(), item->GetHash());
  histogram_tester.ExpectBucketCount("Download.InterruptedReason",
                                     ToHistogramSample<DownloadInterruptReason>(
                                         DOWNLOAD_INTERRUPT_REASON_FILE_FAILED),
                                     1);
}

TEST_F(DownloadItemTest, DestinationCompleted) {
  base::HistogramTester histogram_tester;
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  base::WeakPtr<DownloadDestinationObserver> as_observer(
      item->DestinationObserverAsWeakPtr());
  TestDownloadItemObserver observer(item);

  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_FALSE(item->AllDataSaved());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());

  as_observer->DestinationUpdate(10, 20,
                                 std::vector<DownloadItem::ReceivedSlice>());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_FALSE(observer.CheckAndResetDownloadUpdated());  // Confirm reset.
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_EQ("", item->GetHash());
  EXPECT_FALSE(item->AllDataSaved());

  std::unique_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(kTestData1, sizeof(kTestData1));

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(_, _));
  as_observer->DestinationCompleted(10, std::move(hash));
  mock_delegate()->VerifyAndClearExpectations();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(observer.CheckAndResetDownloadUpdated());
  EXPECT_EQ(
      std::string(std::begin(kHashOfTestData1), std::end(kHashOfTestData1)),
      item->GetHash());
  EXPECT_TRUE(item->AllDataSaved());

  // Even though the DownloadItem receives a DestinationCompleted()
  // event, target determination hasn't completed, hence the download item is
  // stuck in TARGET_PENDING.
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);

  histogram_tester.ExpectTotalCount("Download.InterruptedReason", 0);
}

TEST_F(DownloadItemTest, EnabledActionsForNormalDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // InProgress
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());

  // Complete
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_TRUE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForTemporaryDownload) {
  // A download created with a non-empty FilePath is considered a temporary
  // download.
  create_info()->save_info->file_path = base::FilePath(kDummyTargetPath);
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // InProgress Temporary
  ASSERT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  ASSERT_TRUE(item->IsTemporary());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());

  // Complete Temporary
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(DownloadItem::COMPLETE, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForInterruptedDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->DestinationObserverAsWeakPtr()->DestinationError(
      DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, 0,
      std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();

  ASSERT_EQ(DownloadItem::INTERRUPTED, item->GetState());
  ASSERT_FALSE(item->GetTargetFilePath().empty());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_TRUE(item->CanOpenDownload());
}

TEST_F(DownloadItemTest, EnabledActionsForCancelledDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  EXPECT_CALL(*download_file, Cancel());
  item->Cancel(true);
  task_environment_.RunUntilIdle();

  ASSERT_EQ(DownloadItem::CANCELLED, item->GetState());
  EXPECT_FALSE(item->CanShowInFolder());
  EXPECT_FALSE(item->CanOpenDownload());
}

// Test various aspects of the delegate completion blocker.

// Just allowing completion.
TEST_F(DownloadItemTest, CompleteDelegate_ReturnTrue) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(base::FilePath(kDummyTargetPath), _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion.
TEST_F(DownloadItemTest, CompleteDelegate_BlockOnce) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.

  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  base::OnceClosure delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(DoAll(MoveArg<1>(&delegate_callback), Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  std::move(delegate_callback).Run();
  ASSERT_FALSE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(base::FilePath(kDummyTargetPath), _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Delay and set danger.
TEST_F(DownloadItemTest, CompleteDelegate_SetDanger) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::OnceClosure delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(DoAll(MoveArg<1>(&delegate_callback), Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(delegate_callback);
  EXPECT_FALSE(item->IsDangerous());
  item->OnContentCheckCompleted(DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE,
                                DOWNLOAD_INTERRUPT_REASON_NONE);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  std::move(delegate_callback).Run();
  ASSERT_FALSE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  // Make sure the download doesn't complete until we've validated it.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(base::FilePath(kDummyTargetPath), _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_TRUE(item->IsDangerous());

  item->ValidateDangerousDownload();
  EXPECT_EQ(DOWNLOAD_DANGER_TYPE_USER_VALIDATED, item->GetDangerType());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

// Just delaying completion twice.
TEST_F(DownloadItemTest, CompleteDelegate_BlockTwice) {
  // Test to confirm that if we have a callback that returns true,
  // we complete immediately.
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);

  // Drive the delegate interaction.
  base::OnceClosure delegate_callback;
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(DoAll(MoveArg<1>(&delegate_callback), Return(false)))
      .WillOnce(DoAll(MoveArg<1>(&delegate_callback), Return(false)))
      .WillOnce(Return(true));
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  ASSERT_TRUE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  std::move(delegate_callback).Run();
  ASSERT_TRUE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  std::move(delegate_callback).Run();
  ASSERT_FALSE(delegate_callback);
  EXPECT_EQ(DownloadItem::IN_PROGRESS, item->GetState());
  EXPECT_FALSE(item->IsDangerous());

  // Make sure the download can complete.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(base::FilePath(kDummyTargetPath), _, _, _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldOpenDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(DownloadItem::COMPLETE, item->GetState());
}

TEST_F(DownloadItemTest, CopyDownload) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_DANGEROUS_FILE);
  ASSERT_TRUE(item->IsDangerous());
  base::FilePath full_path(FILE_PATH_LITERAL("foo.txt"));
  base::FilePath returned_path;
  EXPECT_CALL(*download_file, FullPath()).WillOnce(ReturnRefOfCopy(full_path));
  base::WeakPtrFactory<DownloadItemTest> weak_ptr_factory(this);
  item->OnAllDataSaved(0, std::unique_ptr<crypto::SecureHash>());
  item->CopyDownload(base::BindOnce(&DownloadItemTest::OnDownloadFileAcquired,
                                    weak_ptr_factory.GetWeakPtr(),
                                    base::Unretained(&returned_path)));
  task_environment_.RunUntilIdle();
  EXPECT_NE(full_path, returned_path);
  CleanupItem(item, download_file, DownloadItem::IN_PROGRESS);
}

// Tests that for an incognito download, the target file is annotated with an
// empty source URL.
TEST_F(DownloadItemTest, AnnotationWithEmptyURLInIncognito) {
  // Non-incognito case
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  // Target file should be annotated with the source URL.
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(*download_file,
              RenameAndAnnotate(_, _, create_info()->url(), _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();

  // Incognito case
  item = CreateDownloadItem();
  download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  // Target file should be annotated with an empty URL.
  EXPECT_CALL(*download_file, RenameAndAnnotate(_, _, GURL(), _, _, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));

  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*mock_delegate(), IsOffTheRecord()).WillRepeatedly(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
}

TEST_F(DownloadItemTest, AnnotationWithRequestInitiator) {
  DownloadItemImpl* item = CreateDownloadItem();
  MockDownloadFile* download_file =
      DoIntermediateRename(item, DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS);
  auto task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  EXPECT_CALL(
      *download_file,
      RenameAndAnnotate(_, _, _, _, create_info()->request_initiator, _, _))
      .WillOnce(
          WithArg<6>([&task_runner](DownloadFile::RenameCompletionCallback cb) {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(std::move(cb), DOWNLOAD_INTERRUPT_REASON_NONE,
                               base::FilePath(kDummyTargetPath)));
          }));
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(item, _))
      .WillOnce(Return(true));
  EXPECT_CALL(*download_file, FullPath())
      .WillOnce(ReturnRefOfCopy(base::FilePath()));
  EXPECT_CALL(*download_file, Detach());
  item->DestinationObserverAsWeakPtr()->DestinationCompleted(
      0, std::unique_ptr<crypto::SecureHash>());
  task_environment_.RunUntilIdle();
}

// The DownloadItemDestinationUpdateRaceTest fixture (defined below) is used to
// test for race conditions between download destination events received via the
// DownloadDestinationObserver interface, and the target determination logic.
//
// The general control flow for DownloadItemImpl looks like this:
//
// * Start() called, which in turn calls DownloadFile::Initialize().
//
//   Even though OnDownloadFileInitialized hasn't been called, there could now
//   be destination observer calls queued prior to the task that calls
//   OnDownloadFileInitialized. Let's call this point in the workflow "A".
//
// * DownloadItemImpl::OnDownloadFileInitialized() called.
//
// * Assuming the result is successful, DII now invokes the delegate's
//   DetermineDownloadTarget method.
//
//   At this point DownloadFile acts as the source of
//   DownloadDestinationObserver events, and may invoke callbacks. Let's call
//   this point in the workflow "B".
//
// * DII::OnDownloadTargetDetermined() invoked after delegate is done with
//   target determination.
//
// * DII attempts to rename the DownloadFile to its intermediate name.
//
//   More DownloadDestinationObserver events can happen here. Let's call this
//   point in the workflow "C".
//
// * DII::OnDownloadRenamedToIntermediateName() invoked. Assuming all went well,
//   DII is now in IN_PROGRESS state.
//
//   More DownloadDestinationObserver events can happen here. Let's call this
//   point in the workflow "D".
//
// The DownloadItemDestinationUpdateRaceTest works by generating various
// combinations of DownloadDestinationObserver events that might occur at the
// points "A", "B", "C", and "D" above. Each test in this suite cranks a
// DownloadItemImpl through the states listed above and invokes the events
// assigned to each position.

// This type of callback represents a call to a DownloadDestinationObserver
// method that's missing the DownloadDestinationObserver object. Currying this
// way allows us to bind a call prior to constructing the object on which the
// method would be invoked.  This is necessary since we are going to construct
// various permutations of observer calls that will then be applied to a
// DownloadItem in a state as yet undetermined.
using CurriedObservation =
    base::RepeatingCallback<void(base::WeakPtr<DownloadDestinationObserver>)>;

// A list of observations that are to be made during some event in the
// DownloadItemImpl control flow. Ordering of the observations is significant.
using ObservationList = base::circular_deque<CurriedObservation>;

// An ordered list of events.
//
// An "event" in this context refers to some stage in the DownloadItemImpl's
// workflow described as "A", "B", "C", or "D" above. An EventList is expected
// to always contains kEventCount events.
using EventList = base::circular_deque<ObservationList>;

// Number of events in an EventList. This is always 4 for now as described
// above.
const int kEventCount = 4;

// The following functions help us with currying the calls to
// DownloadDestinationObserver. If std::bind was allowed along with
// std::placeholders, it is possible to avoid these functions, but currently
// Chromium doesn't allow using std::bind for good reasons.
void DestinationUpdateInvoker(
    int64_t bytes_so_far,
    int64_t bytes_per_sec,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationUpdate(bytes_so_far:" << bytes_so_far
            << ", bytes_per_sec:" << bytes_per_sec
            << ") observer:" << !!observer;
  if (observer) {
    observer->DestinationUpdate(bytes_so_far, bytes_per_sec,
                                std::vector<DownloadItem::ReceivedSlice>());
  }
}

void DestinationErrorInvoker(
    DownloadInterruptReason reason,
    int64_t bytes_so_far,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationError(reason:"
            << DownloadInterruptReasonToString(reason)
            << ", bytes_so_far:" << bytes_so_far << ") observer:" << !!observer;
  if (observer)
    observer->DestinationError(reason, bytes_so_far,
                               std::unique_ptr<crypto::SecureHash>());
}

void DestinationCompletedInvoker(
    int64_t total_bytes,
    base::WeakPtr<DownloadDestinationObserver> observer) {
  DVLOG(20) << "DestinationComplete(total_bytes:" << total_bytes
            << ") observer:" << !!observer;
  if (observer)
    observer->DestinationCompleted(total_bytes,
                                   std::unique_ptr<crypto::SecureHash>());
}

// Given a set of observations (via the range |begin|..|end|), constructs a list
// of EventLists such that:
//
// * There are exactly |event_count| ObservationSets in each EventList.
//
// * Each ObservationList in each EventList contains a subrange (possibly empty)
//   of observations from the input range, in the same order as the input range.
//
// * The ordering of the ObservationList in each EventList is such that all
//   observations in one ObservationList occur earlier than all observations in
//   an ObservationList that follows it.
//
// * The list of EventLists together describe all the possible ways in which the
//   list of observations can be distributed into |event_count| events.
std::vector<EventList> DistributeObservationsIntoEvents(
    const std::vector<CurriedObservation>::iterator begin,
    const std::vector<CurriedObservation>::iterator end,
    int event_count) {
  std::vector<EventList> all_event_lists;
  for (auto partition = begin;; ++partition) {
    ObservationList first_group_of_observations(begin, partition);
    if (event_count > 1) {
      std::vector<EventList> list_of_subsequent_events =
          DistributeObservationsIntoEvents(partition, end, event_count - 1);
      for (const auto& subsequent_events : list_of_subsequent_events) {
        EventList event_list;
        event_list = subsequent_events;
        event_list.push_front(first_group_of_observations);
        all_event_lists.push_back(event_list);
      }
    } else {
      EventList event_list;
      event_list.push_front(first_group_of_observations);
      all_event_lists.push_back(event_list);
    }
    if (partition == end)
      break;
  }
  return all_event_lists;
}

// For the purpose of this tests, we are only concerned with 3 events:
//
// 1. Immediately after the DownloadFile is initialized.
// 2. Immediately after the DownloadTargetCallback is invoked.
// 3. Immediately after the intermediate file is renamed.
//
// We are going to take a couple of sets of DownloadDestinationObserver events
// and distribute them into the three events described above. And then we are
// going to invoke the observations while a DownloadItemImpl is carefully
// stepped through its stages.

std::vector<EventList> GenerateSuccessfulEventLists() {
  std::vector<CurriedObservation> all_observations;
  all_observations.push_back(base::BindRepeating(&DestinationUpdateInvoker, 100, 100));
  all_observations.push_back(base::BindRepeating(&DestinationUpdateInvoker, 200, 100));
  all_observations.push_back(base::BindRepeating(&DestinationCompletedInvoker, 200));
  return DistributeObservationsIntoEvents(all_observations.begin(),
                                          all_observations.end(), kEventCount);
}

std::vector<EventList> GenerateFailingEventLists() {
  std::vector<CurriedObservation> all_observations;
  all_observations.push_back(base::BindRepeating(&DestinationUpdateInvoker, 100, 100));
  all_observations.push_back(base::BindRepeating(
      &DestinationErrorInvoker, DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, 100));
  return DistributeObservationsIntoEvents(all_observations.begin(),
                                          all_observations.end(), kEventCount);
}

class DownloadItemDestinationUpdateRaceTest
    : public DownloadItemTest,
      public ::testing::WithParamInterface<EventList> {
 public:
  DownloadItemDestinationUpdateRaceTest()
      : item_(CreateDownloadItem()),
        file_(new ::testing::StrictMock<MockDownloadFile>()) {
    DCHECK_EQ(GetParam().size(), static_cast<unsigned>(kEventCount));
  }

 protected:
  const ObservationList& PreInitializeFileObservations() {
    return GetParam().front();
  }
  const ObservationList& PostInitializeFileObservations() {
    return *(GetParam().begin() + 1);
  }
  const ObservationList& PostTargetDeterminationObservations() {
    return *(GetParam().begin() + 2);
  }
  const ObservationList& PostIntermediateRenameObservations() {
    return *(GetParam().begin() + 3);
  }

  // Apply all the observations in |observations| to |observer|, but do so
  // asynchronously so that the events are applied in order behind any tasks
  // that are already scheduled.
  void ScheduleObservations(
      const ObservationList& observations,
      base::WeakPtr<DownloadDestinationObserver> observer) {
    for (const auto& action : observations)
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(action, observer));
  }

  raw_ptr<DownloadItemImpl> item_;
  std::unique_ptr<MockDownloadFile> file_;
};

INSTANTIATE_TEST_SUITE_P(Success,
                         DownloadItemDestinationUpdateRaceTest,
                         ::testing::ValuesIn(GenerateSuccessfulEventLists()));

INSTANTIATE_TEST_SUITE_P(Failure,
                         DownloadItemDestinationUpdateRaceTest,
                         ::testing::ValuesIn(GenerateFailingEventLists()));

// Run through the DII workflow but the embedder cancels the download at target
// determination.
TEST_P(DownloadItemDestinationUpdateRaceTest, DownloadCancelledByUser) {
  // Expect that the download file and the request will be cancelled as a
  // result.
  EXPECT_CALL(*file_, Cancel());

  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_, _, _))
      .WillOnce(SaveArg<0>(&initialize_callback));
  item_->Start(
      std::move(file_),
      base::BindOnce(&DownloadItemTest::CancelRequest, base::Unretained(this)),
      *create_info(), URLLoaderFactoryProvider::GetNullPtr());
  task_environment_.RunUntilIdle();

  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  task_environment_.RunUntilIdle();

  download::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(_, _))
      .WillOnce(MoveArg<1>(&target_callback));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  std::move(initialize_callback).Run(DOWNLOAD_INTERRUPT_REASON_NONE, 0);

  task_environment_.RunUntilIdle();

  ASSERT_TRUE(target_callback);
  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  std::move(target_callback).Run(download::DownloadTargetInfo());
  EXPECT_EQ(DownloadItem::CANCELLED, item_->GetState());
  EXPECT_TRUE(canceled());
  task_environment_.RunUntilIdle();
}

// Run through the DII workflow, but the intermediate rename fails.
TEST_P(DownloadItemDestinationUpdateRaceTest, IntermediateRenameFails) {
  // Expect that the download file and the request will be cancelled as a
  // result.
  EXPECT_CALL(*file_, Cancel());

  // Intermediate rename loop is not used immediately, but let's set up the
  // DownloadFile expectations since we are about to transfer its ownership to
  // the DownloadItem.
  DownloadFile::RenameCompletionCallback intermediate_rename_callback;
  EXPECT_CALL(*file_, RenameAndUniquify(_, _))
      .WillOnce(MoveArg<1>(&intermediate_rename_callback));
  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_, _, _))
      .WillOnce(SaveArg<0>(&initialize_callback));

  item_->Start(
      std::move(file_),
      base::BindOnce(&DownloadItemTest::CancelRequest, base::Unretained(this)),
      *create_info(), URLLoaderFactoryProvider::GetNullPtr());
  task_environment_.RunUntilIdle();

  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  task_environment_.RunUntilIdle();

  download::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(_, _))
      .WillOnce(MoveArg<1>(&target_callback));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  std::move(initialize_callback).Run(DOWNLOAD_INTERRUPT_REASON_NONE, 0);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(target_callback);

  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(kDummyTargetPath);
  target_info.intermediate_path = base::FilePath(kDummyIntermediatePath);
  std::move(target_callback).Run(std::move(target_info));

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(intermediate_rename_callback.is_null());

  ScheduleObservations(PostIntermediateRenameObservations(),
                       destination_observer);
  std::move(intermediate_rename_callback)
      .Run(DOWNLOAD_INTERRUPT_REASON_FILE_FAILED, base::FilePath());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(DownloadItem::INTERRUPTED, item_->GetState());
  EXPECT_TRUE(canceled());
}

// Run through the DII workflow. Download file initialization, target
// determination and intermediate rename all succeed.
TEST_P(DownloadItemDestinationUpdateRaceTest, IntermediateRenameSucceeds) {
  // We expect either that the download will fail (in which case the request and
  // the download file will be cancelled), or it will succeed (in which case the
  // DownloadFile will Detach()). It depends on the list of observations that
  // are given to us.
  EXPECT_CALL(*file_, Cancel()).Times(::testing::AnyNumber());
  EXPECT_CALL(*file_, Detach()).Times(::testing::AnyNumber());

  EXPECT_CALL(*file_, FullPath())
      .WillRepeatedly(ReturnRefOfCopy(base::FilePath(kDummyIntermediatePath)));

  // Intermediate rename loop is not used immediately, but let's set up the
  // DownloadFile expectations since we are about to transfer its ownership to
  // the DownloadItem.
  DownloadFile::RenameCompletionCallback intermediate_rename_callback;
  EXPECT_CALL(*file_, RenameAndUniquify(_, _))
      .WillOnce(MoveArg<1>(&intermediate_rename_callback));

  DownloadFile::InitializeCallback initialize_callback;
  EXPECT_CALL(*file_, Initialize(_, _, _))
      .WillOnce(SaveArg<0>(&initialize_callback));

  item_->Start(std::move(file_), base::DoNothing(), *create_info(),
               URLLoaderFactoryProvider::GetNullPtr());
  task_environment_.RunUntilIdle();

  base::WeakPtr<DownloadDestinationObserver> destination_observer =
      item_->DestinationObserverAsWeakPtr();

  ScheduleObservations(PreInitializeFileObservations(), destination_observer);
  task_environment_.RunUntilIdle();

  download::DownloadTargetCallback target_callback;
  EXPECT_CALL(*mock_delegate(), DetermineDownloadTarget_(_, _))
      .WillOnce(MoveArg<1>(&target_callback));
  ScheduleObservations(PostInitializeFileObservations(), destination_observer);
  std::move(initialize_callback).Run(DOWNLOAD_INTERRUPT_REASON_NONE, 0);

  task_environment_.RunUntilIdle();
  ASSERT_TRUE(target_callback);

  ScheduleObservations(PostTargetDeterminationObservations(),
                       destination_observer);
  download::DownloadTargetInfo target_info;
  target_info.target_path = base::FilePath(kDummyTargetPath);
  target_info.intermediate_path = base::FilePath(kDummyIntermediatePath);
  std::move(target_callback).Run(std::move(target_info));

  task_environment_.RunUntilIdle();
  ASSERT_FALSE(intermediate_rename_callback.is_null());

  // This may or may not be called, depending on whether there are any errors in
  // our action list.
  EXPECT_CALL(*mock_delegate(), ShouldCompleteDownload_(_, _))
      .Times(::testing::AnyNumber());

  ScheduleObservations(PostIntermediateRenameObservations(),
                       destination_observer);
  std::move(intermediate_rename_callback)
      .Run(DOWNLOAD_INTERRUPT_REASON_NONE,
           base::FilePath(kDummyIntermediatePath));
  task_environment_.RunUntilIdle();

  // The state of the download depends on the observer events that were played
  // back to the DownloadItemImpl. Hence we can't establish a single expectation
  // here. On Debug builds, the DCHECKs will verify that the state transitions
  // were correct. On Release builds, tests are expected to run to completion
  // without crashing on success.
  EXPECT_TRUE(item_->GetState() == DownloadItem::IN_PROGRESS ||
              item_->GetState() == DownloadItem::INTERRUPTED);
  if (item_->GetState() == DownloadItem::INTERRUPTED)
    EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, item_->GetLastReason());

  item_->Cancel(true);
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace download
