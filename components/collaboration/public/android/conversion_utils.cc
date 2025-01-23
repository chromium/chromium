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

jlong GetJavaResultCallbackPtr(
    CollaborationControllerDelegate::ResultCallback result) {
  std::unique_ptr<CollaborationControllerDelegate::ResultCallback>
      wrapped_callback =
          std::make_unique<CollaborationControllerDelegate::ResultCallback>(
              std::move(result));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

std::unique_ptr<CollaborationControllerDelegate::ResultCallback>
GetNativeResultCallbackFromJava(jlong callback) {
  return base::WrapUnique(
      reinterpret_cast<CollaborationControllerDelegate::ResultCallback*>(
          callback));
}

jlong GetJavaExitCallbackPtr(base::OnceClosure callback) {
  std::unique_ptr<base::OnceClosure> wrapped_callback =
      std::make_unique<base::OnceClosure>(std::move(callback));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

jlong GetJavaResultWithGroupTokenCallbackPtr(
    ResultWithGroupTokenCallback result) {
  std::unique_ptr<ResultWithGroupTokenCallback> wrapped_callback =
      std::make_unique<ResultWithGroupTokenCallback>(std::move(result));
  CHECK(wrapped_callback.get());
  jlong j_native_ptr = reinterpret_cast<jlong>(wrapped_callback.get());
  wrapped_callback.release();

  return j_native_ptr;
}

std::unique_ptr<base::OnceClosure> GetNativeExitCallbackFromJava(
    jlong callback) {
  return base::WrapUnique(reinterpret_cast<base::OnceClosure*>(callback));
}

std::unique_ptr<ResultWithGroupTokenCallback>
GetNativeResultWithGroupTokenCallbackFromJava(jlong callback) {
  return base::WrapUnique(
      reinterpret_cast<ResultWithGroupTokenCallback*>(callback));
}

jlong GetJavaDelegateUniquePtr(
    std::unique_ptr<CollaborationControllerDelegate> delegate) {
  CollaborationControllerDelegate* delegate_ptr = delegate.get();
  long java_ptr = reinterpret_cast<jlong>(delegate_ptr);
  delegate.release();

  return java_ptr;
}

std::unique_ptr<CollaborationControllerDelegate> GetDelegateUniquePtrFromJava(
    jlong java_ptr) {
  return base::WrapUnique(
      reinterpret_cast<CollaborationControllerDelegate*>(java_ptr));
}

}  // namespace collaboration::conversion
