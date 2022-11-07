// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_postlogin_params.h"

#include <string>

#include "base/files/file_util.h"
#include "chromeos/startup/startup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

// Reads and parses the post-login data to BrowserPostLoginParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserPostLoginParamsPtr ReadStartupBrowserPostLoginParams() {
  absl::optional<std::string> content = ReadPostLoginData();
  if (!content)
    return {};

  crosapi::mojom::BrowserPostLoginParamsPtr result;
  if (!crosapi::mojom::BrowserPostLoginParams::Deserialize(
          content->data(), content->size(), &result)) {
    LOG(ERROR) << "Failed to parse post-login data";
    return {};
  }

  return result;
}

}  // namespace

BrowserPostLoginParams::BrowserPostLoginParams()
    : postlogin_params_(ReadStartupBrowserPostLoginParams()) {
  if (!postlogin_params_) {
    LOG(WARNING) << "BrowserPostLoginParams is not set. "
                 << "This message should not appear except for testing.";
    postlogin_params_ = crosapi::mojom::BrowserPostLoginParams::New();
  }
}

// static
void BrowserPostLoginParams::SetPostLoginParamsForTests(
    crosapi::mojom::BrowserPostLoginParamsPtr postlogin_params) {
  GetInstance()->postlogin_params_ = std::move(postlogin_params);
}

// static
base::ScopedFD BrowserPostLoginParams::CreatePostLoginData() {
  DCHECK(GetInstance()->postlogin_params_);
  return chromeos::CreateMemFDFromBrowserPostLoginParams(
      GetInstance()->postlogin_params_);
}

// static
const crosapi::mojom::BrowserPostLoginParams* BrowserPostLoginParams::Get() {
  return GetInstance()->postlogin_params_.get();
}

// static
BrowserPostLoginParams* BrowserPostLoginParams::GetInstance() {
  static base::NoDestructor<BrowserPostLoginParams> browser_postlogin_params;
  return browser_postlogin_params.get();
}

}  // namespace chromeos
