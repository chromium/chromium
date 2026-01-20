// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/public/android/conversion_utils.h"

#include "base/memory/ptr_util.h"

namespace collaboration::conversion {

namespace {
using Outcome = CollaborationControllerDelegate::Outcome;
using ResultWithGroupTokenCallback = collaboration::
    CollaborationControllerDelegate::ResultWithGroupTokenCallback;

}  // namespace

int64_t GetJavaResultCallbackPtr(
    CollaborationControllerDelegate::ResultCallback result) {
  std::unique_ptr<CollaborationControllerDelegate::ResultCallback>
      wrapped_callback =
          std::make_unique<CollaborationControllerDelegate::ResultCallback>(
              std::move(result));
  CHECK(wrapped_callback.get());
  int64_t j_native_ptr = reinterpret_cast<int64_t>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

std::unique_ptr<CollaborationControllerDelegate::ResultCallback>
GetNativeResultCallbackFromJava(int64_t callback) {
  return base::WrapUnique(
      reinterpret_cast<CollaborationControllerDelegate::ResultCallback*>(
          callback));
}

int64_t GetJavaExitCallbackPtr(base::OnceClosure callback) {
  std::unique_ptr<base::OnceClosure> wrapped_callback =
      std::make_unique<base::OnceClosure>(std::move(callback));
  CHECK(wrapped_callback.get());
  int64_t j_native_ptr = reinterpret_cast<int64_t>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

int64_t GetJavaResultWithGroupTokenCallbackPtr(
    ResultWithGroupTokenCallback result) {
  std::unique_ptr<ResultWithGroupTokenCallback> wrapped_callback =
      std::make_unique<ResultWithGroupTokenCallback>(std::move(result));
  CHECK(wrapped_callback.get());
  int64_t j_native_ptr = reinterpret_cast<int64_t>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

std::unique_ptr<base::OnceClosure> GetNativeExitCallbackFromJava(
    int64_t callback) {
  return base::WrapUnique(reinterpret_cast<base::OnceClosure*>(callback));
}

std::unique_ptr<ResultWithGroupTokenCallback>
GetNativeResultWithGroupTokenCallbackFromJava(int64_t callback) {
  return base::WrapUnique(
      reinterpret_cast<ResultWithGroupTokenCallback*>(callback));
}

int64_t GetJavaDelegateUniquePtr(
    std::unique_ptr<CollaborationControllerDelegate> delegate) {
  CollaborationControllerDelegate* delegate_ptr = delegate.get();
  long java_ptr = reinterpret_cast<int64_t>(delegate_ptr);
  delegate.release();

  return java_ptr;
}

std::unique_ptr<CollaborationControllerDelegate> GetDelegateUniquePtrFromJava(
    int64_t java_ptr) {
  return base::WrapUnique(
      reinterpret_cast<CollaborationControllerDelegate*>(java_ptr));
}

}  // namespace collaboration::conversion
