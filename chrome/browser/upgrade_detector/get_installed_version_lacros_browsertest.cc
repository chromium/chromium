// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/get_installed_version.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/crosapi/mojom/browser_version.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "chromeos/startup/browser_init_params.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_test.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

constexpr char kSampleOldBrowserVersion[] = "95.0.0.0";

class MockBrowserVersionService : public crosapi::mojom::BrowserVersionService {
 public:
  // crosapi::mojom::BrowserVersionService:
  MOCK_METHOD(
      void,
      AddBrowserVersionObserver,
      (mojo::PendingRemote<crosapi::mojom::BrowserVersionObserver> observer),
      (override));

  MOCK_METHOD(void,
              GetInstalledBrowserVersion,
              (GetInstalledBrowserVersionCallback callback),
              (override));
};

}  // namespace

class GetInstalledVersionLacrosBrowserTest : public InProcessBrowserTest {
 public:
  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Replace the production browser version service with a mock for testing.
    mojo::Remote<crosapi::mojom::BrowserVersionService>& remote =
        chromeos::LacrosService::Get()
            ->GetRemote<crosapi::mojom::BrowserVersionService>();
    remote.reset();
    receiver_.Bind(remote.BindNewPipeAndPassReceiver());
  }

  // Returns a reference to the mocked browser version `service_`.
  testing::NiceMock<MockBrowserVersionService>& service() { return service_; }

 private:
  testing::NiceMock<MockBrowserVersionService> service_;
  mojo::Receiver<crosapi::mojom::BrowserVersionService> receiver_{&service_};
};

IN_PROC_BROWSER_TEST_F(
    GetInstalledVersionLacrosBrowserTest,
    DefaultToRunningRootfsVersionWhenOlderStatefulBrowserComponentVersion) {
  auto params = crosapi::mojom::BrowserInitParams::New();
  params->lacros_selection =
      crosapi::mojom::BrowserInitParams::LacrosSelection::kRootfs;
  chromeos::BrowserInitParams::SetInitParamsForTests(std::move(params));

  base::RunLoop run_loop;

  EXPECT_CALL(service(), GetInstalledBrowserVersion(_))
      .WillOnce([](crosapi::mojom::BrowserVersionService::
                       GetInstalledBrowserVersionCallback callback) {
        std::move(callback).Run(kSampleOldBrowserVersion);
      });

  base::MockCallback<InstalledVersionCallback> callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce([&run_loop](InstalledAndCriticalVersion versions) {
        EXPECT_EQ(version_info::GetVersion(), versions.installed_version);
        EXPECT_FALSE(versions.critical_version.has_value());
        run_loop.Quit();
      });

  GetInstalledVersion(callback.Get());
  run_loop.Run();
}
