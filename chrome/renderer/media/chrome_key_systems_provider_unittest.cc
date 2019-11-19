// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/chrome_key_systems_provider.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/simple_test_tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace {

class TestKeySystemProperties : public media::KeySystemProperties {
 public:
  explicit TestKeySystemProperties(const std::string& key_system_name)
      : key_system_name_(key_system_name) {}

  std::string GetKeySystemName() const override { return key_system_name_; }
  bool IsSupportedInitDataType(
      media::EmeInitDataType init_data_type) const override {
    return false;
  }

  media::EmeConfigRule GetEncryptionSchemeConfigRule(
      media::EncryptionScheme encryption_scheme) const override {
    return media::EmeConfigRule::NOT_SUPPORTED;
  }

  media::SupportedCodecs GetSupportedCodecs() const override {
    return media::EME_CODEC_NONE;
  }

  media::EmeConfigRule GetRobustnessConfigRule(
      media::EmeMediaType media_type,
      const std::string& requested_robustness) const override {
    return requested_robustness.empty() ? media::EmeConfigRule::SUPPORTED
                                        : media::EmeConfigRule::NOT_SUPPORTED;
  }

  media::EmeSessionTypeSupport GetPersistentLicenseSessionSupport()
      const override {
    return media::EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  media::EmeSessionTypeSupport GetPersistentUsageRecordSessionSupport()
      const override {
    return media::EmeSessionTypeSupport::NOT_SUPPORTED;
  }

  media::EmeFeatureSupport GetPersistentStateSupport() const override {
    return media::EmeFeatureSupport::NOT_SUPPORTED;
  }

  media::EmeFeatureSupport GetDistinctiveIdentifierSupport() const override {
    return media::EmeFeatureSupport::NOT_SUPPORTED;
  }

 private:
  const std::string key_system_name_;
};

class TestKeySystemsProviderDelegate {
 public:
  TestKeySystemsProviderDelegate() : include_widevine_(false) {}

  void AddTestKeySystems(
      std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {
    key_systems->emplace_back(
        new TestKeySystemProperties("com.example.foobar"));

    if (include_widevine_) {
#if BUILDFLAG(ENABLE_WIDEVINE)
      key_systems->emplace_back(
          new TestKeySystemProperties(kWidevineKeySystem));
#else
      // Tests should only attempt to include Widevine when it is available.
      NOTREACHED();
#endif
    }
  }

  void set_include_widevine(bool include_widevine) {
    include_widevine_ = include_widevine;
  }

 private:
  bool include_widevine_;
};

}  // namespace

TEST(ChromeKeySystemsProviderTest, IsKeySystemsUpdateNeeded) {
  ChromeKeySystemsProvider key_systems_provider;

  base::SimpleTestTickClock tick_clock;
  key_systems_provider.SetTickClockForTesting(&tick_clock);

  std::unique_ptr<TestKeySystemsProviderDelegate> provider_delegate(
      new TestKeySystemsProviderDelegate());
  key_systems_provider.SetProviderDelegateForTesting(
      base::Bind(&TestKeySystemsProviderDelegate::AddTestKeySystems,
                 base::Unretained(provider_delegate.get())));

  // IsKeySystemsUpdateNeeded() always returns true after construction.
  EXPECT_TRUE(key_systems_provider.IsKeySystemsUpdateNeeded());

  std::vector<std::unique_ptr<media::KeySystemProperties>> key_systems;
  key_systems_provider.AddSupportedKeySystems(&key_systems);

  // No update needed immediately after AddSupportedKeySystems() call.
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());

  // Widevine not initially provided.
  EXPECT_EQ(key_systems.size(), 1U);
  EXPECT_EQ(key_systems[0]->GetKeySystemName(), "com.example.foobar");

  // This is timing related. The update interval for Widevine is 1000 ms.
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(990));
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(10));

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  // Require update once enough time has passed for builds that install Widevine
  // as a component.
  EXPECT_TRUE(key_systems_provider.IsKeySystemsUpdateNeeded());

  // Now add Widevine.
  provider_delegate->set_include_widevine(true);
  key_systems.clear();
  key_systems_provider.AddSupportedKeySystems(&key_systems);

  // Widevine should now be among the list.
  bool found_widevine = false;
  for (const auto& key_system_properties : key_systems) {
    if (key_system_properties->GetKeySystemName() == kWidevineKeySystem) {
      found_widevine = true;
      break;
    }
  }
  EXPECT_TRUE(found_widevine);

  // Update not needed now, nor later because Widevine has been described.
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
#else
  // No update needed for builds that either don't offer Widevine or do so
  // as part of Chrome rather than component installer.
  EXPECT_FALSE(key_systems_provider.IsKeySystemsUpdateNeeded());
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
}
