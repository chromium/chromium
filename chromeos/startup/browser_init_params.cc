// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_init_params.h"

#include <string>

#include "chromeos/startup/startup.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

namespace {

// Reads and parses the startup data to BrowserInitParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserInitParamsPtr ReadStartupBrowserInitParams() {
  absl::optional<std::string> content = ReadStartupData();
  if (!content)
    return {};

  crosapi::mojom::BrowserInitParamsPtr result;
  if (!crosapi::mojom::BrowserInitParams::Deserialize(
          content->data(), content->size(), &result)) {
    LOG(ERROR) << "Failed to parse startup data";
    return {};
  }

  return result;
}

}  // namespace

BrowserInitParams::BrowserInitParams()
    : init_params_(disable_crosapi_for_testing_
                       ? crosapi::mojom::BrowserInitParams::New()
                       : ReadStartupBrowserInitParams()) {
  if (!init_params_) {
    LOG(WARNING) << "BrowserInitParams is not set. "
                 << "This message should not appear except for testing. "
                 << "For testing, consider setting "
                 << "BrowserInitParams::disable_crosapi_for_testing_ "
                 << "to true if crosapi is not required.";
    init_params_ = crosapi::mojom::BrowserInitParams::New();
  }
}

// static
const crosapi::mojom::BrowserInitParams* BrowserInitParams::GetForTests() {
  return Get();
}

// static
const crosapi::mojom::BrowserInitParams* BrowserInitParams::Get() {
  return GetInstance()->init_params_.get();
}

// static
void BrowserInitParams::SetInitParamsForTests(
    crosapi::mojom::BrowserInitParamsPtr init_params) {
  GetInstance()->init_params_ = std::move(init_params);
}

// static
base::ScopedFD BrowserInitParams::CreateStartupData() {
  DCHECK(GetInstance()->init_params_);
  return chromeos::CreateMemFDFromBrowserInitParams(
      GetInstance()->init_params_);
}

// static
BrowserInitParams* BrowserInitParams::GetInstance() {
  static base::NoDestructor<BrowserInitParams> browser_init_params;
  return browser_init_params.get();
}

// static
bool BrowserInitParams::disable_crosapi_for_testing_ = false;

}  // namespace chromeos
