// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/native/test/test_upload_data_provider.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helper class that runs base::OnceClosure.
class TestRunnable {
 public:
  // Creates Cronet runnable that runs |task| once and destroys itself.
  static Cronet_RunnablePtr CreateRunnable(base::OnceClosure task);

  TestRunnable(const TestRunnable&) = delete;
  TestRunnable& operator=(const TestRunnable&) = delete;

 private:
  explicit TestRunnable(base::OnceClosure task);
  ~TestRunnable();

  // Runs |self| and destroys it.
  static void Run(Cronet_RunnablePtr self);

  // Closure to run.
  base::OnceClosure task_;
};

TestRunnable::TestRunnable(base::OnceClosure task) : task_(std::move(task)) {}

TestRunnable::~TestRunnable() = default;

// static
Cronet_RunnablePtr TestRunnable::CreateRunnable(base::OnceClosure task) {
  Cronet_RunnablePtr runnable = Cronet_Runnable_CreateWith(TestRunnable::Run);
  Cronet_Runnable_SetClientContext(runnable, new TestRunnable(std::move(task)));
  return runnable;
}

// static
void TestRunnable::Run(Cronet_RunnablePtr self) {
  CHECK(self);
  Cronet_ClientContext context = Cronet_Runnable_GetClientContext(self);
  TestRunnable* runnable = static_cast<TestRunnable*>(context);
  CHECK(runnable);
  std::move(runnable->task_).Run();
  delete runnable;
}

}  // namespace

namespace cronet {
// Various test utility functions for testing Cronet.
namespace test {

TestUploadDataProvider::TestUploadDataProvider(
    SuccessCallbackMode success_callback_mode,
    Cronet_ExecutorPtr executor)
    : success_callback_mode_(success_callback_mode), executor_(executor) {}

TestUploadDataProvider::~TestUploadDataProvider() = default;

Cronet_UploadDataProviderPtr
TestUploadDataProvider::CreateUploadDataProvider() {
  Cronet_UploadDataProviderPtr upload_data_provider =
      Cronet_UploadDataProvider_CreateWith(
          TestUploadDataProvider::GetLength, TestUploadDataProvider::Read,
          TestUploadDataProvider::Rewind, TestUploadDataProvider::Close);
  Cronet_UploadDataProvider_SetClientContext(upload_data_provider, this);
  return upload_data_provider;
}

void TestUploadDataProvider::AddRead(std::string read) {
  EXPECT_TRUE(!started_) << "Adding bytes after read";
  reads_.push_back(read);
}

void TestUploadDataProvider::SetReadFailure(int read_fail_index,
                                            FailMode read_fail_mode) {
  read_fail_index_ = read_fail_index;
  read_fail_mode_ = read_fail_mode;
}

void TestUploadDataProvider::SetRewindFailure(FailMode rewind_fail_mode) {
  rewind_fail_mode_ = rewind_fail_mode;
}

void TestUploadDataProvider::SetReadCancel(int read_cancel_index,
                                           CancelMode read_cancel_mode) {
  read_cancel_index_ = read_cancel_index;
  read_cancel_mode_ = read_cancel_mode;
}

void TestUploadDataProvider::SetRewindCancel(CancelMode rewind_cancel_mode) {
  rewind_cancel_mode_ = rewind_cancel_mode;
}

int64_t TestUploadDataProvider::GetLength() const {
  EXPECT_TRUE(!closed_.IsSet()) << "Data Provider is closed";
  if (bad_length_ != -1)
    return bad_length_;

  return GetUploadedLength();
}

int64_t TestUploadDataProvider::GetUploadedLength() const {
  if (chunked_)
    return -1ll;

  int64_t length = 0ll;
  for (const auto& read : reads_)
    length += read.size();

  return length;
}

void TestUploadDataProvider::Read(Cronet_UploadDataSinkPtr upload_data_sink,
                                  Cronet_BufferPtr buffer) {
  int current_read_call = num_read_calls_;
  ++num_read_calls_;
  EXPECT_TRUE(!closed_.IsSet()) << "Data Provider is closed";

  AssertIdle();

  if (current_read_call == read_cancel_index_)
    MaybeCancelRequest(read_cancel_mode_);

  if (MaybeFailRead(current_read_call, upload_data_sink)) {
    failed_ = true;
    return;
  }

  read_pending_ = true;
  started_ = true;

  bool final_chunk = (chunked_ && next_read_ == reads_.size() - 1);
  EXPECT_TRUE(next_read_ < reads_.size()) << "Too many reads: " << next_read_;
  const auto& read = reads_[next_read_];
  EXPECT_TRUE(read.size() <= Cronet_Buffer_GetSize(buffer))
      << "Read buffer smaller than expected.";
  memcpy(Cronet_Buffer_GetData(buffer), read.data(), read.size());
  ++next_read_;

  auto complete_closure = base::BindOnce(
      [](TestUploadDataProvider* upload_data_provider,
         Cronet_UploadDataSink* upload_data_sink, uint64_t bytes_read,
         bool final_chunk) {
        upload_data_provider->read_pending_ = false;
        Cronet_UploadDataSink_OnReadSucceeded(upload_data_sink, bytes_read,
                                              final_chunk);
      },
      this, upload_data_sink, read.size(), final_chunk);

  if (success_callback_mode_ == SYNC) {
    std::move(complete_closure).Run();
  } else {
    PostTaskToExecutor(std::move(complete_closure));
  }
}

void TestUploadDataProvider::Rewind(Cronet_UploadDataSinkPtr upload_data_sink) {
  ++num_rewind_calls_;
  EXPECT_TRUE(!closed_.IsSet()) << "Data Provider is closed";
  AssertIdle();

  MaybeCancelRequest(rewind_cancel_mode_);

  if (MaybeFailRewind(upload_data_sink)) {
    failed_ = true;
    return;
  }

  // Should never try and rewind when rewinding does nothing.
  EXPECT_TRUE(next_read_ != 0) << "Unexpected rewind when already at beginning";

  rewind_pending_ = true;
  next_read_ = 0;

  auto complete_closure = base::BindOnce(
      [](TestUploadDataProvider* upload_data_provider,
         Cronet_UploadDataSink* upload_data_sink) {
        upload_data_provider->rewind_pending_ = false;
        Cronet_UploadDataSink_OnRewindSucceeded(upload_data_sink);
      },
      this, upload_data_sink);

  if (success_callback_mode_ == SYNC) {
    std::move(complete_closure).Run();
  } else {
    PostTaskToExecutor(std::move(complete_closure));
  }
}

void TestUploadDataProvider::PostTaskToExecutor(base::OnceClosure task) {
  EXPECT_TRUE(executor_);
  // |runnable| is passed to executor, which destroys it after execution.
  Cronet_Executor_Execute(executor_,
                          TestRunnable::CreateRunnable(std::move(task)));
}

void TestUploadDataProvider::AssertIdle() const {
  EXPECT_TRUE(!read_pending_) << "Unexpected operation during read";
  EXPECT_TRUE(!rewind_pending_) << "Unexpected operation during rewind";
  EXPECT_TRUE(!failed_) << "Unexpected operation after failure";
}

bool TestUploadDataProvider::MaybeFailRead(
    int read_index,
    Cronet_UploadDataSinkPtr upload_data_sink) {
  if (read_fail_mode_ == NONE)
    return false;
  if (read_index != read_fail_index_)
    return false;

  if (read_fail_mode_ == CALLBACK_SYNC) {
    Cronet_UploadDataSink_OnReadError(upload_data_sink, "Sync read failure");
    return true;
  }
  EXPECT_EQ(read_fail_mode_, CALLBACK_ASYNC);

  PostTaskToExecutor(base::BindOnce(
      [](Cronet_UploadDataSink* upload_data_sink) {
        Cronet_UploadDataSink_OnReadError(upload_data_sink,
                                          "Async read failure");
      },
      upload_data_sink));
  return true;
}

bool TestUploadDataProvider::MaybeFailRewind(
    Cronet_UploadDataSinkPtr upload_data_sink) {
  if (rewind_fail_mode_ == NONE)
    return false;

  if (rewind_fail_mode_ == CALLBACK_SYNC) {
    Cronet_UploadDataSink_OnRewindError(upload_data_sink,
                                        "Sync rewind failure");
    return true;
  }
  EXPECT_EQ(rewind_fail_mode_, CALLBACK_ASYNC);

  PostTaskToExecutor(base::BindOnce(
      [](Cronet_UploadDataSink* upload_data_sink) {
        Cronet_UploadDataSink_OnRewindError(upload_data_sink,
                                            "Async rewind failure");
      },
      upload_data_sink));
  return true;
}

void TestUploadDataProvider::MaybeCancelRequest(CancelMode cancel_mode) {
  if (cancel_mode == CANCEL_NONE)
    return;

  CHECK(url_request_);

  if (cancel_mode == CANCEL_SYNC) {
    Cronet_UrlRequest_Cancel(url_request_);
    return;
  }

  EXPECT_EQ(cancel_mode, CANCEL_ASYNC);
  PostTaskToExecutor(base::BindOnce(
      [](Cronet_UrlRequestPtr url_request) {
        Cronet_UrlRequest_Cancel(url_request);
      },
      url_request_));
}

void TestUploadDataProvider::Close() {
  EXPECT_TRUE(!closed_.IsSet()) << "Closed twice";
  closed_.Set();
  awaiting_close_.Signal();
}

void TestUploadDataProvider::AssertClosed() {
  awaiting_close_.TimedWait(base::Milliseconds(5000));
  EXPECT_TRUE(closed_.IsSet()) << "Was not closed";
}

/* static */
TestUploadDataProvider* TestUploadDataProvider::GetThis(
    Cronet_UploadDataProviderPtr self) {
  return static_cast<TestUploadDataProvider*>(
      Cronet_UploadDataProvider_GetClientContext(self));
}

/* static */
int64_t TestUploadDataProvider::GetLength(Cronet_UploadDataProviderPtr self) {
  return GetThis(self)->GetLength();
}

/* static */
void TestUploadDataProvider::Read(Cronet_UploadDataProviderPtr self,
                                  Cronet_UploadDataSinkPtr upload_data_sink,
                                  Cronet_BufferPtr buffer) {
  return GetThis(self)->Read(upload_data_sink, buffer);
}

/* static */
void TestUploadDataProvider::Rewind(Cronet_UploadDataProviderPtr self,
                                    Cronet_UploadDataSinkPtr upload_data_sink) {
  return GetThis(self)->Rewind(upload_data_sink);
}

/* static */
void TestUploadDataProvider::Close(Cronet_UploadDataProviderPtr self) {
  return GetThis(self)->Close();
}

}  // namespace test
}  // namespace cronet
