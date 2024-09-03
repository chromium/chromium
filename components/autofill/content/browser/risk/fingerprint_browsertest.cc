// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/risk/fingerprint.h"

#include <stdint.h>

#include <memory>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/common/content_autofill_features.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"
#include "services/device/public/mojom/geoposition.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen_info.h"
#include "ui/gfx/geometry/rect.h"

using testing::ElementsAre;

namespace autofill {
namespace risk {

namespace internal {

// Defined in the implementation file corresponding to this test.
void GetFingerprintInternal(
    uint64_t obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const display::ScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    base::Time install_time,
    const std::string& app_locale,
    const std::string& user_agent,
    base::TimeDelta timeout,
    base::OnceCallback<void(std::unique_ptr<Fingerprint>)> callback);

}  // namespace internal

// Constants that are passed verbatim to the fingerprinter code and should be
// serialized into the resulting protocol buffer.
const uint64_t kObfuscatedGaiaId = UINT64_C(16571487432910023183);
const char kCharset[] = "UTF-8";
const char kAcceptLanguages[] = "en-US,en";
const int kScreenColorDepth = 53;
const char kLocale[] = "en-GB";
const char kUserAgent[] = "TestUserAgent";

// Geolocation constants that are passed verbatim to the fingerprinter code and
// should be serialized into the resulting protocol buffer.
const double kLatitude = -42.0;
const double kLongitude = 17.3;
const double kAltitude = 123.4;
const double kAccuracy = 73.7;
const int kGeolocationTime = 87;

class AutofillRiskFingerprintTest : public content::ContentBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  AutofillRiskFingerprintTest()
      : window_bounds_(2, 3, 5, 7),
        content_bounds_(11, 13, 17, 37),
        screen_bounds_(0, 0, 101, 71),
        available_screen_bounds_(0, 11, 101, 60),
        unavailable_screen_bounds_(0, 0, 101, 11) {
    scoped_feature_list_.InitWithFeatureState(
        features::kAutofillDisableGeolocationInRiskFingerprint,
        IsGeolocationDisabled());
  }

  void SetUpOnMainThread() override {
    auto position = device::mojom::Geoposition::New();
    position->latitude = kLatitude;
    position->longitude = kLongitude;
    position->altitude = kAltitude;
    position->accuracy = kAccuracy;
    position->timestamp =
        base::Time::UnixEpoch() + base::Milliseconds(kGeolocationTime);

    geolocation_overrider_ =
        std::make_unique<device::ScopedGeolocationOverrider>(
            device::mojom::GeopositionResult::NewPosition(std::move(position)));
  }

  void GetFingerprintTestCallback(base::OnceClosure continuation_callback,
                                  std::unique_ptr<Fingerprint> fingerprint) {
    // Verify that all fields Chrome can fill have been filled.
    ASSERT_TRUE(fingerprint->has_machine_characteristics());
    const Fingerprint::MachineCharacteristics& machine =
        fingerprint->machine_characteristics();
    EXPECT_TRUE(machine.has_operating_system_build());
    EXPECT_TRUE(machine.has_browser_install_time_hours());

#if BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)
    // GetFontList() returns an empty list on Fuchsia and Android.
    EXPECT_EQ(machine.font_size(), 0);
#else
    EXPECT_GT(machine.font_size(), 0);
#endif  // BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_ANDROID)

    // TODO(isherman): http://crbug.com/358548 and EXPECT_EQ.
    EXPECT_GE(machine.plugin_size(), 0);

    EXPECT_TRUE(machine.has_utc_offset_ms());
    EXPECT_TRUE(machine.has_browser_language());
    EXPECT_GT(machine.requested_language_size(), 0);
    EXPECT_TRUE(machine.has_charset());
    EXPECT_TRUE(machine.has_screen_count());
    ASSERT_TRUE(machine.has_screen_size());
    EXPECT_TRUE(machine.screen_size().has_width());
    EXPECT_TRUE(machine.screen_size().has_height());
    EXPECT_TRUE(machine.has_screen_color_depth());
    ASSERT_TRUE(machine.has_unavailable_screen_size());
    EXPECT_TRUE(machine.unavailable_screen_size().has_width());
    EXPECT_TRUE(machine.unavailable_screen_size().has_height());
    EXPECT_TRUE(machine.has_user_agent());
    ASSERT_TRUE(machine.has_cpu());
    EXPECT_TRUE(machine.cpu().has_vendor_name());
    EXPECT_TRUE(machine.cpu().has_brand());
    EXPECT_TRUE(machine.has_ram());
    EXPECT_TRUE(machine.has_browser_build());
    EXPECT_TRUE(machine.has_browser_feature());
    if (content::GpuDataManager::GetInstance()->GpuAccessAllowed(nullptr)) {
      EXPECT_TRUE(machine.has_graphics_card());
    }
    if (machine.has_graphics_card()) {
      EXPECT_TRUE(machine.graphics_card().has_vendor_id());
      EXPECT_TRUE(machine.graphics_card().has_device_id());
    }

    ASSERT_TRUE(fingerprint->has_transient_state());
    const Fingerprint::TransientState& transient_state =
        fingerprint->transient_state();
    ASSERT_TRUE(transient_state.has_inner_window_size());
    ASSERT_TRUE(transient_state.has_outer_window_size());
    EXPECT_TRUE(transient_state.inner_window_size().has_width());
    EXPECT_TRUE(transient_state.inner_window_size().has_height());
    EXPECT_TRUE(transient_state.outer_window_size().has_width());
    EXPECT_TRUE(transient_state.outer_window_size().has_height());

    if (IsGeolocationDisabled()) {
      ASSERT_FALSE(fingerprint->has_user_characteristics());
    } else {
      ASSERT_TRUE(fingerprint->has_user_characteristics());
      const Fingerprint::UserCharacteristics& user_characteristics =
          fingerprint->user_characteristics();
      ASSERT_TRUE(user_characteristics.has_location());
      const Fingerprint::UserCharacteristics::Location& location =
          user_characteristics.location();
      EXPECT_TRUE(location.has_altitude());
      EXPECT_TRUE(location.has_latitude());
      EXPECT_TRUE(location.has_longitude());
      EXPECT_TRUE(location.has_accuracy());
      EXPECT_TRUE(location.has_time_in_ms());

      EXPECT_EQ(kAltitude, location.altitude());
      EXPECT_EQ(kLatitude, location.latitude());
      EXPECT_EQ(kLongitude, location.longitude());
      EXPECT_EQ(kAccuracy, location.accuracy());
      EXPECT_EQ(kGeolocationTime, location.time_in_ms());
    }

    ASSERT_TRUE(fingerprint->has_metadata());
    EXPECT_TRUE(fingerprint->metadata().has_timestamp_ms());
    EXPECT_TRUE(fingerprint->metadata().has_obfuscated_gaia_id());
    EXPECT_TRUE(fingerprint->metadata().has_fingerprinter_version());

    // Some values have exact known (mocked out) values:
    EXPECT_THAT(machine.requested_language(), ElementsAre("en-US", "en"));
    EXPECT_EQ(kLocale, machine.browser_language());
    EXPECT_EQ(kUserAgent, machine.user_agent());
    EXPECT_EQ(kCharset, machine.charset());
    EXPECT_EQ(kScreenColorDepth, machine.screen_color_depth());
    EXPECT_EQ(unavailable_screen_bounds_.width(),
              machine.unavailable_screen_size().width());
    EXPECT_EQ(unavailable_screen_bounds_.height(),
              machine.unavailable_screen_size().height());
    EXPECT_EQ(Fingerprint::MachineCharacteristics::FEATURE_REQUEST_AUTOCOMPLETE,
              machine.browser_feature());
    EXPECT_EQ(content_bounds_.width(),
              transient_state.inner_window_size().width());
    EXPECT_EQ(content_bounds_.height(),
              transient_state.inner_window_size().height());
    EXPECT_EQ(window_bounds_.width(),
              transient_state.outer_window_size().width());
    EXPECT_EQ(window_bounds_.height(),
              transient_state.outer_window_size().height());
    EXPECT_EQ(kObfuscatedGaiaId, fingerprint->metadata().obfuscated_gaia_id());

    std::move(continuation_callback).Run();
  }

  bool IsGeolocationDisabled() const { return GetParam(); }

 protected:
  // Constants defining bounds in the screen coordinate system that are passed
  // verbatim to the fingerprinter code and should be serialized into the
  // resulting protocol buffer.  Declared as class members because gfx::Rect is
  // not a POD type, so it cannot be statically initialized.
  const gfx::Rect window_bounds_;
  const gfx::Rect content_bounds_;
  const gfx::Rect screen_bounds_;
  const gfx::Rect available_screen_bounds_;
  const gfx::Rect unavailable_screen_bounds_;

  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that getting a fingerprint works on some basic level.
IN_PROC_BROWSER_TEST_P(AutofillRiskFingerprintTest, GetFingerprint) {
  display::ScreenInfo screen_info;
  screen_info.depth = kScreenColorDepth;
  screen_info.rect = screen_bounds_;
  screen_info.available_rect = available_screen_bounds_;

  base::RunLoop run_loop;
  internal::GetFingerprintInternal(
      kObfuscatedGaiaId, window_bounds_, content_bounds_, screen_info,
      "25.0.0.123", kCharset, kAcceptLanguages, AutofillClock::Now(), kLocale,
      kUserAgent,
      base::Days(1),  // Ought to be longer than any test run.
      base::BindOnce(&AutofillRiskFingerprintTest::GetFingerprintTestCallback,
                     base::Unretained(this), run_loop.QuitWhenIdleClosure()));

  // Wait for the callback to be called.
  run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All, AutofillRiskFingerprintTest, ::testing::Bool());

}  // namespace risk
}  // namespace autofill
