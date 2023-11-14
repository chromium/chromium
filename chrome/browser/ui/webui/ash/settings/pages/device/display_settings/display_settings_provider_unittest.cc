// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

namespace {

// A mock observer that records current tablet mode status and counts when
// OnTabletModeChanged function is called.
class FakeTabletModeObserver : public mojom::TabletModeObserver {
 public:
  uint32_t num_tablet_mode_change_calls() const {
    return num_tablet_mode_change_calls_;
  }

  bool is_tablet_mode() { return is_tablet_mode_; }

  // mojom::TabletModeObserver:
  void OnTabletModeChanged(bool is_tablet_mode) override {
    ++num_tablet_mode_change_calls_;
    is_tablet_mode_ = is_tablet_mode;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForTabletModeChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::TabletModeObserver> receiver{this};

 private:
  uint32_t num_tablet_mode_change_calls_ = 0;
  bool is_tablet_mode_ = false;
  base::OnceClosure quit_callback_;
};

// A mock observer that counts when ObserveDisplayConfiguration function is
// called.
class FakeDisplayConfigurationObserver
    : public mojom::DisplayConfigurationObserver {
 public:
  uint32_t num_display_configuration_changed_calls() const {
    return num_display_configuration_changed_calls_;
  }

  // mojom::DisplayConfigurationObserver:
  void OnDisplayConfigurationChanged() override {
    ++num_display_configuration_changed_calls_;

    if (quit_callback_) {
      std::move(quit_callback_).Run();
    }
  }

  void WaitForDisplayConfigurationChanged() {
    DCHECK(quit_callback_.is_null());
    base::RunLoop loop;
    quit_callback_ = loop.QuitClosure();
    loop.Run();
  }

  mojo::Receiver<mojom::DisplayConfigurationObserver> receiver{this};

 private:
  uint32_t num_display_configuration_changed_calls_ = 0;
  base::OnceClosure quit_callback_;
};

}  // namespace

class DisplaySettingsProviderTest : public ChromeAshTestBase {
 public:
  DisplaySettingsProviderTest() = default;
  ~DisplaySettingsProviderTest() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    provider_ = std::make_unique<DisplaySettingsProvider>();
  }

  void TearDown() override {
    provider_.reset();
    ChromeAshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<DisplaySettingsProvider> provider_;
};

// Test the behavior when the tablet mode status has changed. The tablet mode is
// initialized as "not-in-tablet-mode".
TEST_F(DisplaySettingsProviderTest, TabletModeObservation) {
  FakeTabletModeObserver fake_observer;
  base::test::TestFuture<bool> future;

  // Attach a tablet mode observer.
  provider_->ObserveTabletMode(
      fake_observer.receiver.BindNewPipeAndPassRemote(), future.GetCallback());
  base::RunLoop().RunUntilIdle();

  // Default initial state is "not-in-tablet-mode".
  ASSERT_FALSE(future.Get<0>());

  provider_->OnTabletModeEventsBlockingChanged();
  fake_observer.WaitForTabletModeChanged();

  EXPECT_EQ(1u, fake_observer.num_tablet_mode_change_calls());
}

// Test the behavior when the display configuration has changed.
TEST_F(DisplaySettingsProviderTest, DisplayConfigurationObservation) {
  FakeDisplayConfigurationObserver fake_observer;

  // Attach a display configuration observer.
  provider_->ObserveDisplayConfiguration(
      fake_observer.receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  provider_->OnDidProcessDisplayChanges(/*configuration_change=*/{{}, {}, {}});
  fake_observer.WaitForDisplayConfigurationChanged();

  EXPECT_EQ(1u, fake_observer.num_display_configuration_changed_calls());
}

}  // namespace ash::settings
