// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_INTEGRITY_SERVICE_BRIDGE_H_
#define COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_INTEGRITY_SERVICE_BRIDGE_H_

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"

namespace environment_integrity {

// Response codes for calling the Environment Integrity API.
//
// GENERATED_JAVA_ENUM_PACKAGE: (
// org.chromium.components.environment_integrity.enums)
enum class IntegrityResponse {
  kSuccess,
  kUnknownError,
  kApiNotAvailable,
  kTimeout,
  kInvalidHandle,
};

// Result of calling `CreateIntegrityHandle`.
struct HandleCreationResult {
  IntegrityResponse response_code;
  int64_t handle;
  std::string error_message;
};

// Result of calling `GetEnvironmentIntegrityToken`.
struct GetTokenResult {
  GetTokenResult();
  ~GetTokenResult();
  GetTokenResult(GetTokenResult&&);
  GetTokenResult& operator=(GetTokenResult&&);

  IntegrityResponse response_code;
  std::vector<uint8_t> token;
  std::string error_message;
};

using CreateHandleCallback = base::OnceCallback<void(HandleCreationResult)>;
using GetTokenCallback = base::OnceCallback<void(GetTokenResult)>;

// Wrapper for the java EnvironmentIntegrityService.java which can be swapped
// out for testing.
class IntegrityService {
 public:
  IntegrityService() = default;
  virtual ~IntegrityService() = default;
  // Check if integrity services are available.
  // Use this quick static check to determine if it is worth calling any other
  // integrity service functions.
  virtual bool IsIntegrityAvailable();

  // Create a new integrity handle.
  //
  // The callback will always be called. Check the `response_code` property to
  // determine if the value in `handle` is valid.
  virtual void CreateIntegrityHandle(CreateHandleCallback callback);

  // Get an environment integrity token for the given `content_binding`.
  //
  // The callback will always be called. Check the `response_code` property to
  // determine if the value in `token` is valid.
  virtual void GetEnvironmentIntegrityToken(
      int64_t handle,
      const std::vector<uint8_t>& content_binding,
      GetTokenCallback callback);
};

}  // namespace environment_integrity
#endif  // COMPONENTS_ENVIRONMENT_INTEGRITY_ANDROID_INTEGRITY_SERVICE_BRIDGE_H_
