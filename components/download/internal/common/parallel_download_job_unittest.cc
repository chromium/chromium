// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/common/parallel_download_job.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "components/download/internal/common/parallel_download_utils.h"
#include "components/download/public/common/download_create_info.h"
#include "components/download/public/common/download_destination_observer.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/mock_download_item.h"
#include "components/download/public/common/mock_input_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::StrictMock;

namespace download {

namespace {

class MockDownloadDestinationObserver : public DownloadDestinationObserver {
 public:
  MOCK_METHOD3(DestinationUpdate,
               void(int64_t,
                    int64_t,
                    const std::vector<DownloadItem::ReceivedSlice>&));
  void DestinationError(
      DownloadInterruptReason reason,
      int64_t bytes_so_far,
      std::unique_ptr<crypto::SecureHash> hash_state) override {}
  void DestinationCompleted(
      int64_t total_bytes,
      std::unique_ptr<crypto::SecureHash> hash_state) override {}
  MOCK_METHOD2(CurrentUpdateStatus, void(int64_t, int64_t));
};

}  // namespace

class ParallelDownloadJobForTest : public ParallelDownloadJob {
 public:
  ParallelDownloadJobForTest(
      DownloadItem* download_item,
      DownloadJob::CancelRequestCallback cancel_request_callback,
      const DownloadCreateInfo& create_info,
      int request_count,
      int64_t min_slice_size,
      int min_remaining_time)
      : ParallelDownloadJob(download_item,
                            std::move(cancel_request_callback),
                            create_info,
                            URLLoaderFactoryProvider::GetNullPtr(),
                            nullptr),
        request_count_(request_count),
        min_slice_size_(min_slice_size),
        min_remaining_time_(min_remaining_time) {}

  void CreateRequest(int64_t offset) override {
    auto worker = std::make_unique<DownloadWorker>(this, offset);

    DCHECK(workers_.find(offset) == workers_.end());
    workers_[offset] = std::move(worker);
  }

  ParallelDownloadJob::WorkerMap& workers() { return workers_; }

  void MakeFileInitialized(DownloadFile::InitializeCallback callback,
                           DownloadInterruptReason result) {
    ParallelDownloadJob::OnDownloadFileInitialized(std::move(callback), result,
                                                   0);
  }

  int GetParallelRequestCount() const override { return request_count_; }
  int64_t GetMinSliceSize() const override { return min_slice_size_; }
  int GetMinRemainingTimeInSeconds() const override {
    return min_remaining_time_;
  }

  void OnInputStreamReady(
      DownloadWorker* worker,
      std::unique_ptr<InputStream> input_stream,
      std::unique_ptr<DownloadCreateInfo> download_create_info) override {
    CountOnInputStreamReady();
  }

  MOCK_METHOD0(CountOnInputStreamReady, void());

 private:
  int request_count_;
  int min_slice_size_;
  int min_remaining_time_;
  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJobForTest);
};

class ParallelDownloadJobTest : public testing::Test {
 public:
  ParallelDownloadJobTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::UI,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {}

  void CreateParallelJob(int64_t initial_request_offset,
                         int64_t content_length,
                         const DownloadItem::ReceivedSlices& slices,
                         int request_count,
                         int64_t min_slice_size,
                         int min_remaining_time) {
    received_slices_ = slices;
    download_item_ = std::make_unique<NiceMock<MockDownloadItem>>();
    EXPECT_CALL(*download_item_, GetTotalBytes())
        .WillRepeatedly(Return(initial_request_offset + content_length));
    EXPECT_CALL(*download_item_, GetReceivedBytes())
        .WillRepeatedly(Return(initial_request_offset));
    EXPECT_CALL(*download_item_, GetReceivedSlices())
        .WillRepeatedly(ReturnRef(received_slices_));

    DownloadCreateInfo info;
    info.offset = initial_request_offset;
    info.total_bytes = content_length;
    job_ = std::make_unique<ParallelDownloadJobForTest>(
        download_item_.get(),
        base::Bind(&ParallelDownloadJobTest::CancelRequest,
                   base::Unretained(this)),
        info, request_count, min_slice_size, min_remaining_time);
    file_initialized_ = false;
  }

  void DestroyParallelJob() {
    job_.reset();
    download_item_.reset();
  }

  void BuildParallelRequests() { job_->BuildParallelRequests(); }

  void set_received_slices(const DownloadItem::ReceivedSlices& slices) {
    received_slices_ = slices;
  }

  bool IsJobCanceled() const { return job_->is_canceled_; }

  void CancelRequest(bool user_cancel) { canceled_ = true; }

  void VerifyWorker(int64_t offset, int64_t length) const {
    EXPECT_TRUE(job_->workers_.find(offset) != job_->workers_.end());
    EXPECT_EQ(offset, job_->workers_[offset]->offset());
  }

  void OnFileInitialized(DownloadInterruptReason result, int64_t bytes_wasted) {
    file_initialized_ = true;
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockDownloadItem> download_item_;
  std::unique_ptr<ParallelDownloadJobForTest> job_;
  bool file_initialized_;
  bool canceled_ = false;

  // The received slices used to return in
  // |MockDownloadItemImpl::GetReceivedSlices| mock function.
  DownloadItem::ReceivedSlices received_slices_;
};

// Test if parallel requests can be built correctly for a new download without
// existing slices.
TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequestsWithoutSlices) {
  // Totally 2 requests for 100 bytes.
  // Original request:  Range:0-, for 50 bytes.
  // Task 1:  Range:50-, for 50 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(50, 0);
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes.
  // Original request:  Range:0-, for 33 bytes.
  // Task 1:  Range:33-, for 33 bytes.
  // Task 2:  Range:66-, for 34 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(2u, job_->workers().size());
  VerifyWorker(33, 0);
  VerifyWorker(66, 0);
  DestroyParallelJob();

  // Less than 2 requests, do nothing.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 1, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 0, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // Content-length is 0, do nothing.
  CreateParallelJob(0, 0, DownloadItem::ReceivedSlices(), 3, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();
}

TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequestsWithSlices) {
  // File size: 100 bytes.
  // Received slices: [0, 17]
  // Original request: Range:12-. Content-length: 88.
  // Totally 3 requests for 83 bytes.
  // Original request:  Range:12-43.
  // Task 1:  Range:44-70, for 27 bytes.
  // Task 2:  Range:71-, for 29 bytes.
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 17)};
  CreateParallelJob(12, 88, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(2u, job_->workers().size());
  VerifyWorker(44, 0);
  VerifyWorker(71, 0);
  DestroyParallelJob();

  // File size: 100 bytes.
  // Received slices: [0, 60], Range:0-59.
  // Original request: Range:60-. Content-length: 40.
  // 40 bytes left for 4 requests. Only 1 additional request.
  // Original request: Range:60-79, for 20 bytes.
  // Task 1:  Range:80-, for 20 bytes.
  slices = {DownloadItem::ReceivedSlice(0, 60)};
  CreateParallelJob(60, 40, slices, 4, 20, 10);
  BuildParallelRequests();
  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(80, 0);
  DestroyParallelJob();

  // Content-Length is 0, no additional requests.
  slices = {DownloadItem::ReceivedSlice(0, 100)};
  CreateParallelJob(100, 0, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // File size: 100 bytes.
  // Original request: Range:0-. Content-length: 12(Incorrect server header).
  // The request count is 2, however the file contains 3 holes, and we don't
  // know if the last slice is completed, so there should be 3 requests in
  // parallel and the last request is an out-of-range request.
  slices = {
      DownloadItem::ReceivedSlice(10, 10), DownloadItem::ReceivedSlice(20, 10),
      DownloadItem::ReceivedSlice(40, 10), DownloadItem::ReceivedSlice(90, 10)};
  CreateParallelJob(0, 12, slices, 2, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(3u, job_->workers().size());
  VerifyWorker(30, 0);
  VerifyWorker(50, 0);
  VerifyWorker(100, 0);
  DestroyParallelJob();
}

// Ensure that in download resumption, if the first hole is filled before
// sending multiple requests, the new requests can be correctly calculated.
TEST_F(ParallelDownloadJobTest, CreateResumptionRequestsFirstSliceFilled) {
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 10),
                                         DownloadItem::ReceivedSlice(40, 10),
                                         DownloadItem::ReceivedSlice(80, 10)};

  // The updated slices that has filled the first hole.
  DownloadItem::ReceivedSlices updated_slices = slices;
  updated_slices[0].received_bytes = 40;

  CreateParallelJob(10, 90, slices, 3, 1, 10);
  // Now let download item to return an updated received slice, that the first
  // hole in the file has been filled.
  set_received_slices(updated_slices);
  BuildParallelRequests();

  // Since the first hole is filled, parallel requests are created to fill other
  // two holes.
  EXPECT_EQ(2u, job_->workers().size());
  VerifyWorker(50, 0);
  VerifyWorker(90, 0);
  DestroyParallelJob();
}

// Simulate an edge case that we have one received slice in the middle. The
// parallel request should be created correctly.
// This may not happen under current implementation, but should be also handled
// correctly.
TEST_F(ParallelDownloadJobTest, CreateResumptionRequestsTwoSlicesToFill) {
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(40, 10)};

  CreateParallelJob(0, 100, slices, 3, 1, 10);
  BuildParallelRequests();

  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(50, 0);
  DestroyParallelJob();

  DownloadItem::ReceivedSlices updated_slices = {
      DownloadItem::ReceivedSlice(0, 10), DownloadItem::ReceivedSlice(40, 10)};

  CreateParallelJob(0, 100, slices, 3, 1, 10);
  // Now let download item to return an updated received slice, that the first
  // hole in the file is not fully filled.
  set_received_slices(updated_slices);
  BuildParallelRequests();

  // Because the initial request is working on the first hole, there should be
  // only one parallel request to fill the second hole.
  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(50, 0);
  DestroyParallelJob();
}

// Verifies that if the last received slice is finished, we don't send an out
// of range request that starts from the last byte position.
TEST_F(ParallelDownloadJobTest, LastReceivedSliceFinished) {
  // One finished slice, no parallel requests should be created. Content length
  // should be 0.
  DownloadItem::ReceivedSlices slices = {
      DownloadItem::ReceivedSlice(0, 100, true)};
  CreateParallelJob(100, 0, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(0u, job_->workers().size());
  DestroyParallelJob();

  // Two received slices with one hole in the middle. Since the second slice is
  // finished, and the hole will be filled by original request, no parallel
  // requests will be created.
  slices = {DownloadItem::ReceivedSlice(0, 25),
            DownloadItem::ReceivedSlice(75, 25, true)};
  CreateParallelJob(25, 100, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(0u, job_->workers().size());
  DestroyParallelJob();

  // Three received slices with two hole in the middle and the last slice is
  // finished. The original request will work on the first hole and one parallel
  // request is created to fill the second hole.
  slices = {DownloadItem::ReceivedSlice(0, 25),
            DownloadItem::ReceivedSlice(50, 25),
            DownloadItem::ReceivedSlice(100, 25, true)};
  CreateParallelJob(25, 125, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(75, 0);
  DestroyParallelJob();

  // Three received slices with two hole in the middle and the last slice is
  // finished.
  slices = {DownloadItem::ReceivedSlice(0, 25),
            DownloadItem::ReceivedSlice(50, 25),
            DownloadItem::ReceivedSlice(100, 25, true)};
  CreateParallelJob(25, 125, slices, 3, 1, 10);

  // If the first hole is filled by the original request after the job is
  // initialized but before parallel request is created, the second hole should
  // be filled, and no out of range request will be created.
  slices[0].received_bytes = 50;
  set_received_slices(slices);
  BuildParallelRequests();
  EXPECT_EQ(1u, job_->workers().size());
  VerifyWorker(75, 0);
  DestroyParallelJob();
}

// Pause, cancel, resume can be called before or after the worker establish
// the byte stream.
// These tests ensure the states consistency between the job and workers.

// Ensure cancel before building the requests will result in no requests are
// built.
TEST_F(ParallelDownloadJobTest, EarlyCancelBeforeBuildRequests) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 1, 10);

  // Job is canceled before building parallel requests.
  job_->Cancel(true);
  EXPECT_TRUE(IsJobCanceled());
  EXPECT_TRUE(canceled_);

  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());

  DestroyParallelJob();
}

// Test that parallel request is not created if the remaining content can be
// finish downloading soon.
TEST_F(ParallelDownloadJobTest, RemainingContentWillFinishSoon) {
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 99)};
  CreateParallelJob(99, 1, slices, 3, 1, 10);
  BuildParallelRequests();
  EXPECT_EQ(0u, job_->workers().size());

  DestroyParallelJob();
}

// Test that parallel request is not created until download file is initialized.
TEST_F(ParallelDownloadJobTest, ParallelRequestNotCreatedUntilFileInitialized) {
  auto save_info = std::make_unique<DownloadSaveInfo>();
  StrictMock<MockInputStream>* input_stream = new StrictMock<MockInputStream>();
  auto observer =
      std::make_unique<StrictMock<MockDownloadDestinationObserver>>();
  base::WeakPtrFactory<DownloadDestinationObserver> observer_factory(
      observer.get());
  auto download_file = std::make_unique<DownloadFileImpl>(
      std::move(save_info), base::FilePath(),
      std::unique_ptr<MockInputStream>(input_stream), DownloadItem::kInvalidId,
      observer_factory.GetWeakPtr());
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2, 0, 0);
  job_->Start(download_file.get(),
              base::Bind(&ParallelDownloadJobTest::OnFileInitialized,
                         base::Unretained(this)),
              DownloadItem::ReceivedSlices());
  EXPECT_FALSE(file_initialized_);
  EXPECT_EQ(0u, job_->workers().size());
  EXPECT_CALL(*input_stream, RegisterDataReadyCallback(_));
  EXPECT_CALL(*input_stream, Read(_, _));
  EXPECT_CALL(*(observer.get()), DestinationUpdate(_, _, _));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(file_initialized_);
  EXPECT_EQ(1u, job_->workers().size());
  DestroyParallelJob();

  // The download file lives on the download sequence, and must
  // be deleted there.
  GetDownloadTaskRunner()->DeleteSoon(FROM_HERE, std::move(download_file));
  task_environment_.RunUntilIdle();
}

// Interruption from IO thread after the file initialized and before building
// the parallel requests, should correctly stop the download.
TEST_F(ParallelDownloadJobTest, InterruptOnStartup) {
  DownloadItem::ReceivedSlices slices = {DownloadItem::ReceivedSlice(0, 99)};
  CreateParallelJob(99, 1, slices, 3, 1, 10);

  // Start to build the requests without any error.
  base::MockCallback<DownloadFile::InitializeCallback> callback;
  EXPECT_CALL(callback, Run(_, _)).Times(1);
  job_->MakeFileInitialized(callback.Get(), DOWNLOAD_INTERRUPT_REASON_NONE);

  // Simulate and inject an error from IO thread after file initialized.
  EXPECT_CALL(*download_item_, GetState())
      .WillRepeatedly(Return(DownloadItem::DownloadState::INTERRUPTED));

  // Because of the error, no parallel requests are built.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, job_->workers().size());

  DestroyParallelJob();
}

}  // namespace download
