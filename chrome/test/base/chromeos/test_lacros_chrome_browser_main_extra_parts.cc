// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test_lacros_chrome_browser_main_extra_parts.h"

#include "base/logging.h"
#include "chrome/test/chromeos/standalone_browser_test_controller.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/crosapi/mojom/test_controller.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace test {

TestLacrosChromeBrowserMainExtraParts::TestLacrosChromeBrowserMainExtraParts() =
    default;

TestLacrosChromeBrowserMainExtraParts::
    ~TestLacrosChromeBrowserMainExtraParts() = default;

void TestLacrosChromeBrowserMainExtraParts::PostBrowserStart() {
  auto* lacros_service = chromeos::LacrosService::Get();
  CHECK(lacros_service->IsAvailable<crosapi::mojom::TestController>());
  auto& ash_test_controller =
      lacros_service->GetRemote<crosapi::mojom::TestController>();
  standalone_browser_test_controller_ =
      std::make_unique<StandaloneBrowserTestController>(ash_test_controller);
}

}  // namespace test
