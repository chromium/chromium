// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_postlogin_params.h"

#include <string>

#include "base/check_is_test.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
#include "chromeos/startup/startup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

// Reads and parses the post-login data to BrowserPostLoginParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserPostLoginParamsPtr ReadStartupBrowserPostLoginParams() {
  absl::optional<std::string> content = ReadPostLoginData();
  if (!content) {
    // Ash shut down or crashed, so the pipe is broken.
    // Lacros should shut down gracefully instead of crashing.
    base::Process::TerminateCurrentProcessImmediately(
        RESULT_CODE_INVALID_POST_LOGIN_PARAMS);
  }

  crosapi::mojom::BrowserPostLoginParamsPtr result;
  if (!crosapi::mojom::BrowserPostLoginParams::Deserialize(
          content->data(), content->size(), &result)) {
    LOG(ERROR) << "Failed to parse post-login data";
    return {};
  }

  return result;
}

}  // namespace

// static
void BrowserPostLoginParams::WaitForLogin() {
  // TODO(crbug.com/1475643): added to investigate the cause of this crash.
  // Please remove once the cause is identified.
  LOG(WARNING) << "Waiting for login.";

  auto* instance = GetInstanceInternal();
  if (!instance->postlogin_params_) {
    // Fetch the postlogin parameters, or wait for them to be available.
    instance->postlogin_params_ = ReadStartupBrowserPostLoginParams();
    CHECK(instance->postlogin_params_);
  } else {
    // This code path should only be reached in tests after calling
    // SetPostLoginParamsForTests.
    CHECK_IS_TEST();
  }
}

// static
void BrowserPostLoginParams::SetPostLoginParamsForTests(
    crosapi::mojom::BrowserPostLoginParamsPtr postlogin_params) {
  GetInstanceInternal()->postlogin_params_ = std::move(postlogin_params);
}

// static
base::ScopedFD BrowserPostLoginParams::CreatePostLoginData() {
  CHECK(GetInstanceInternal()->postlogin_params_);
  return chromeos::CreateMemFDFromBrowserPostLoginParams(
      GetInstanceInternal()->postlogin_params_);
}

// static
const crosapi::mojom::BrowserPostLoginParams* BrowserPostLoginParams::Get() {
  CHECK(GetInstanceInternal()->postlogin_params_);
  return GetInstanceInternal()->postlogin_params_.get();
}

// static
BrowserPostLoginParams* BrowserPostLoginParams::GetInstanceInternal() {
  static base::NoDestructor<BrowserPostLoginParams> browser_postlogin_params;
  return browser_postlogin_params.get();
}

BrowserPostLoginParams::BrowserPostLoginParams() = default;

}  // namespace chromeos
