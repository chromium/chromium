// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBSHARE_WIN_FAKE_IASYNC_OPERATION_WITH_PROGRESS_H_
#define CHROME_BROWSER_WEBSHARE_WIN_FAKE_IASYNC_OPERATION_WITH_PROGRESS_H_

#include <wrl/client.h>

#include "base/notreached.h"
#include "base/win/winrt_foundation_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webshare {

namespace internal {

// Templates used to allow easy reference to the correct types.
// See base/win/winrt_foundation_helpers.h for explanation.
template <typename TResult, typename TProgress>
using AsyncOperationWithProgressComplex = typename ABI::Windows::Foundation::
    IAsyncOperationWithProgress<TResult, TProgress>::TResult_complex;

template <typename TResult, typename TProgress>
using AsyncOperationWithProgressAbi = base::win::internal::AbiType<
    AsyncOperationWithProgressComplex<TResult, TProgress>>;

template <typename TResult, typename TProgress>
using AsyncOperationWithProgressOptionalStorage =
    base::win::internal::OptionalStorageType<
        AsyncOperationWithProgressComplex<TResult, TProgress>>;

template <typename TResult, typename TProgress>
using AsyncOperationWithProgressStorage = base::win::internal::StorageType<
    AsyncOperationWithProgressComplex<TResult, TProgress>>;

}  // namespace internal

// Provides an implementation of IAsyncOperationWithProgress for use in GTests.
template <typename TResult, typename TProgress>
class FakeIAsyncOperationWithProgress final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::WinRtClassicComMix>,
          ABI::Windows::Foundation::IAsyncOperationWithProgress<TResult,
                                                                TProgress>,
          ABI::Windows::Foundation::IAsyncInfo> {
 public:
  FakeIAsyncOperationWithProgress() = default;
  FakeIAsyncOperationWithProgress(const FakeIAsyncOperationWithProgress&) =
      delete;
  FakeIAsyncOperationWithProgress& operator=(
      const FakeIAsyncOperationWithProgress&) = delete;

  // ABI::Windows::Foundation::IAsyncOperationWithProgress:
  IFACEMETHODIMP put_Progress(
      ABI::Windows::Foundation::IAsyncOperationProgressHandler<TResult,
                                                               TProgress>*
          handler) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Progress(
      ABI::Windows::Foundation::IAsyncOperationProgressHandler<TResult,
                                                               TProgress>**
          handler) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP put_Completed(
      ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<
          TResult,
          TProgress>* handler) final {
    EXPECT_EQ(nullptr, handler_)
        << "put_Completed called on IAsyncOperation with a CompletedHandler "
           "already defined.";
    handler_ = handler;
    return S_OK;
  }
  IFACEMETHODIMP get_Completed(
      ABI::Windows::Foundation::IAsyncOperationWithProgressCompletedHandler<
          TResult,
          TProgress>** handler) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP GetResults(
      internal::AsyncOperationWithProgressAbi<TResult, TProgress>* results)
      final {
    if (!is_complete_) {
      ADD_FAILURE()
          << "GetResults called on incomplete IAsyncOperationWithProgress.";
      return E_PENDING;
    }
    if (status_ != AsyncStatus::Completed)
      return E_UNEXPECTED;
    return base::win::internal::CopyTo(results_, results);
  }

  // ABI::Windows::Foundation::IAsyncInfo:
  IFACEMETHODIMP get_Id(uint32_t* id) final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP get_Status(AsyncStatus* status) final {
    *status = status_;
    return S_OK;
  }
  IFACEMETHODIMP get_ErrorCode(HRESULT* error_code) final {
    *error_code = error_code_;
    return S_OK;
  }
  IFACEMETHODIMP Cancel() final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }
  IFACEMETHODIMP Close() final {
    NOTREACHED_IN_MIGRATION();
    return E_NOTIMPL;
  }

  // Completes the operation with |error_code|.
  //
  // The get_ErrorCode API will be set to return |error_code|, the remainder of
  // the APIs will be set to represent an error state, and the CompletedHandler
  // (if defined) will be run.
  void CompleteWithError(HRESULT error_code) {
    error_code_ = error_code;
    status_ = AsyncStatus::Error;
    InvokeCompletedHandler();
  }

  // Completes the operation with |results|.
  //
  // The GetResults API will be set to return |results|, the remainder of the
  // APIs will be set to represent a successfully completed state, and the
  // CompletedHandler (if defined) will be run.
  void CompleteWithResults(
      internal::AsyncOperationWithProgressStorage<TResult, TProgress> results) {
    error_code_ = S_OK;
    results_ = std::move(results);
    status_ = AsyncStatus::Completed;
    InvokeCompletedHandler();
  }

 private:
  void InvokeCompletedHandler() {
    ASSERT_FALSE(is_complete_)
        << "Attempted to invoke completion on an already "
           "completed IAsyncOperationWithProgress.";
    is_complete_ = true;
    if (handler_)
      handler_->Invoke(this, status_);
  }

  HRESULT error_code_ = S_OK;
  Microsoft::WRL::ComPtr<
      ABI::Windows::Foundation::
          IAsyncOperationWithProgressCompletedHandler<TResult, TProgress>>
      handler_;
  bool is_complete_ = false;
  internal::AsyncOperationWithProgressOptionalStorage<TResult, TProgress>
      results_;
  AsyncStatus status_ = AsyncStatus::Started;
};

}  // namespace webshare

#endif  // CHROME_BROWSER_WEBSHARE_WIN_FAKE_IASYNC_OPERATION_WITH_PROGRESS_H_
