// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_init_params.h"

#include <optional>
#include <string>

#include "base/check_is_test.h"
#include "base/command_line.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/startup.h"

namespace chromeos {

namespace {

// Reads and parses the startup data to BrowserInitParams.
// If data is missing, or failed to parse, returns a null StructPtr.
crosapi::mojom::BrowserInitParamsPtr ReadStartupBrowserInitParams() {
  std::optional<std::string> content = ReadStartupData();
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

std::optional<bool> BrowserInitParams::is_crosapi_enabled_;

bool BrowserInitParams::IsCrosapiDisabledForTesting() {
  return !IsCrosapiEnabled();
}

bool BrowserInitParams::IsCrosapiEnabled() {
  if (is_crosapi_enabled_.has_value()) {
    return *is_crosapi_enabled_;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  const bool enabled =
      command_line->HasSwitch("crosapi-mojo-platform-channel-handle") ||
      command_line->HasSwitch("lacros-mojo-socket-for-testing");
  if (!enabled) {
    CHECK_IS_TEST();
  }
  is_crosapi_enabled_ = enabled;
  return enabled;
}

void BrowserInitParams::DisableCrosapiForTesting() {
  CHECK_IS_TEST();
  // TODO(crbug.com/324508902): Strengthen this CHECK condition to
  // `!is_crosapi_enabled_.has_value()` when the bug is fixed.
  CHECK(!is_crosapi_enabled_.value_or(false))
      << "You are calling DisableCrosapiForTesting too late.";
  is_crosapi_enabled_ = false;
}

BrowserInitParams::BrowserInitParams()
    : init_params_(IsCrosapiEnabled()
                       ? ReadStartupBrowserInitParams()
                       : crosapi::mojom::BrowserInitParams::New()) {
  if (IsCrosapiEnabled()) {
    CHECK(init_params_) << "crosapi is enabled but BrowserInitParams could not "
                           "be read. You are probably trying to get or set "
                           "the BrowserInitParams too early.";
    CHECK(init_params_->ash_chrome_version);
  } else {
    CHECK(init_params_);
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
  CHECK_IS_TEST();
  if (IsCrosapiEnabled()) {
    CHECK(init_params);
    CHECK(init_params->ash_chrome_version)
        << "crosapi is enabled but the given BrowserInitParams is missing "
           "essential data. Make sure to use "
           "BrowserInitParams::GetForTests()->Clone() and customize that "
           "instead of starting with an empty one";
  }
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

}  // namespace chromeos
