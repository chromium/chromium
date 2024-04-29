// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_postlogin_params.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/files/file_util.h"
#include "base/process/process.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/startup.h"

namespace chromeos {

namespace {

// Reads and parses the post-login data to BrowserPostLoginParams.
// If the data is missing or empty, we didn't receive it from Ash, likely
// because Ash is shutting down and closing its end of the pipe. In that case,
// we terminate Lacros gracefully.
// If the data failed to parse because of a deserialization error, returns a
// null StructPtr.
crosapi::mojom::BrowserPostLoginParamsPtr ReadStartupBrowserPostLoginParams() {
  std::optional<std::string> content = ReadPostLoginData();
  if (!content.has_value() || content->empty()) {
    // Ash shut down or crashed, so the pipe is broken or empty.
    // Lacros should shut down gracefully instead of crashing.
    // See crbug.com/1491478 for additional context.
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
  // TODO(crbug.com/40070475): added to investigate the cause of this crash.
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

  // Signal that the user has logged in.
  instance->logged_in_ = true;
}

// static
bool BrowserPostLoginParams::IsLoggedIn() {
  return GetInstanceInternal()->logged_in_;
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
