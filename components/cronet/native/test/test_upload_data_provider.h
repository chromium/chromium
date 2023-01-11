// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_NATIVE_TEST_TEST_UPLOAD_DATA_PROVIDER_H_
#define COMPONENTS_CRONET_NATIVE_TEST_TEST_UPLOAD_DATA_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cronet_c.h"

#include "base/functional/bind.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cronet {
// Various test utility functions for testing Cronet.
namespace test {

/**
 * An UploadDataProvider implementation used in tests.
 */
class TestUploadDataProvider {
 public:
  // Indicates whether all success callbacks are synchronous or asynchronous.
  // Doesn't apply to errors.
  enum SuccessCallbackMode { SYNC, ASYNC };

  // Indicates whether failures should invoke callbacks synchronously, or
  // invoke callback asynchronously.
  enum FailMode { NONE, CALLBACK_SYNC, CALLBACK_ASYNC };

  // Indicates whether request should be canceled synchronously before
  // the callback or asynchronously after.
  enum CancelMode { CANCEL_NONE, CANCEL_SYNC, CANCEL_ASYNC };

  TestUploadDataProvider(SuccessCallbackMode success_callback_mode,
                         Cronet_ExecutorPtr executor);

  virtual ~TestUploadDataProvider();

  Cronet_UploadDataProviderPtr CreateUploadDataProvider();

  // Adds the result to be returned by a successful read request.  The
  // returned bytes must all fit within the read buffer provided by Cronet.
  // After a rewind, if there is one, all reads will be repeated.
  void AddRead(std::string read);

  void SetReadFailure(int read_fail_index, FailMode read_fail_mode);

  void SetRewindFailure(FailMode rewind_fail_mode);

  void SetReadCancel(int read_cancel_index, CancelMode read_cancel_mode);

  void SetRewindCancel(CancelMode rewind_cancel_mode);

  void set_bad_length(int64_t bad_length) { bad_length_ = bad_length; }

  void set_chunked(bool chunked) { chunked_ = chunked; }

  void set_url_request(Cronet_UrlRequestPtr request) { url_request_ = request; }

  Cronet_ExecutorPtr executor() const { return executor_; }

  int num_read_calls() const { return num_read_calls_; }

  int num_rewind_calls() const { return num_rewind_calls_; }

  /**
   * Returns the cumulative length of all data added by calls to addRead.
   */
  virtual int64_t GetLength() const;

  int64_t GetUploadedLength() const;

  virtual void Read(Cronet_UploadDataSinkPtr upload_data_sink,
                    Cronet_BufferPtr buffer);

  void Rewind(Cronet_UploadDataSinkPtr upload_data_sink);

  void AssertClosed();

 private:
  void PostTaskToExecutor(base::OnceClosure task);

  void AssertIdle() const;

  bool MaybeFailRead(int read_index, Cronet_UploadDataSinkPtr upload_data_sink);

  bool MaybeFailRewind(Cronet_UploadDataSinkPtr upload_data_sink);

  void MaybeCancelRequest(CancelMode cancel_mode);

  void Close();

  // Implementation of Cronet_UploadDataProvider methods.
  static TestUploadDataProvider* GetThis(Cronet_UploadDataProviderPtr self);

  static int64_t GetLength(Cronet_UploadDataProviderPtr self);
  static void Read(Cronet_UploadDataProviderPtr self,
                   Cronet_UploadDataSinkPtr upload_data_sink,
                   Cronet_BufferPtr buffer);
  static void Rewind(Cronet_UploadDataProviderPtr self,
                     Cronet_UploadDataSinkPtr upload_data_sink);
  static void Close(Cronet_UploadDataProviderPtr self);

  std::vector<std::string> reads_;
  const SuccessCallbackMode success_callback_mode_ = SYNC;
  const Cronet_ExecutorPtr executor_;

  Cronet_UrlRequestPtr url_request_;

  bool chunked_ = false;

  // Index of read to fail on.
  int read_fail_index_ = -1;
  // Indicates how to fail on a read.
  FailMode read_fail_mode_ = NONE;
  FailMode rewind_fail_mode_ = NONE;

  // Index of read to cancel on.
  int read_cancel_index_ = -1;
  // Indicates how to cancel on a read.
  CancelMode read_cancel_mode_ = CANCEL_NONE;
  CancelMode rewind_cancel_mode_ = CANCEL_NONE;

  // Report bad length if not set to -1.
  int64_t bad_length_ = -1;

  int num_read_calls_ = 0;
  int num_rewind_calls_ = 0;

  size_t next_read_ = 0;
  bool started_ = false;
  bool read_pending_ = false;
  bool rewind_pending_ = false;
  // Used to ensure there are no read/rewind requests after a failure.
  bool failed_ = false;

  base::AtomicFlag closed_;
  base::WaitableEvent awaiting_close_;
};

}  // namespace test
}  // namespace cronet

#endif  // COMPONENTS_CRONET_NATIVE_TEST_TEST_UPLOAD_DATA_PROVIDER_H_
