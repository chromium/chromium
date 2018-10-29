// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include "base/json/json_reader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/mojom/device.mojom.h"

namespace {

class UsbPolicyAllowedDevicesTest : public testing::Test {
 public:
  UsbPolicyAllowedDevicesTest() {}
  ~UsbPolicyAllowedDevicesTest() override {}

  void SetWebUsbAllowDevicesForUrlsPrefValue(const base::Value& value) {
    profile_.GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls, value);
  }

 protected:
  Profile* profile() { return &profile_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithMissingPrefValue) {
  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithExistingEmptyPrefValue) {
  base::Value pref_value(base::Value::Type::LIST);

  SetWebUsbAllowDevicesForUrlsPrefValue(pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

namespace {

constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 },
          { "vendor_id": 4321 }
        ],
        "url_patterns": [
          "https://[*.]google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{}],
        "url_patterns": ["https://[*.]youtube.com"]
      }
    ])";

constexpr char kPolicySettingWithInvalidUrlPattern[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 }
        ],
        "url_patterns": ["https://badpattern.[*]"]
      }
    ])";

constexpr char kPolicySettingWithInvalidUrlPattern2[] = R"(
    [
      {
        "devices": [
          { "vendor_id": 1234, "product_id": 5678 }
        ],
        "url_patterns": [
          "https://badpattern.[*]",
          "https://[*.]google.com"
        ]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithExistingPrefValue) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& first_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& second_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& third_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      third_url_patterns,
      content_settings::ParsePatternString("https://[*.]youtube.com")));
}

// Entries without valid URL patterns are ignored.
TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPrefValueContainingInvalidUrlPattern) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySettingWithInvalidUrlPattern);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  ASSERT_TRUE(map.empty());
}

// Invalid URL patterns are ignored on entries also containing valid patterns.
TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPrefValueContainingInvalidUrlPattern2) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySettingWithInvalidUrlPattern2);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 1ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
}

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithMissingPolicyThenUpdatePolicy) {
  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());
  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());

  // Ensure that the allowed devices can be dynamically updated.
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& first_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& second_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& third_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      third_url_patterns,
      content_settings::ParsePatternString("https://[*.]youtube.com")));
}

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPolicyThenRemovePolicy) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  ASSERT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& first_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      first_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& second_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      second_url_patterns,
      content_settings::ParsePatternString("https://crbug.com")));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  const auto& third_url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      third_url_patterns,
      content_settings::ParsePatternString("https://[*.]youtube.com")));

  // Ensure that the allowed devices can be removed dynamically.
  pref_value.reset(new base::Value(base::Value::Type::LIST));
  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

namespace {

constexpr char kPolicySettingWithEntriesContainingDuplicateDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "url_patterns": [
          "https://[*.]google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "url_patterns": ["https://[*.]youtube.com"]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPrefValueContainingDuplicateDevices) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::Read(
      kPolicySettingWithEntriesContainingDuplicateDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlPatternsMap& map =
      usb_policy_allowed_devices->map();
  ASSERT_EQ(map.size(), 1ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::ContainsKey(map, device_key));

  // Ensure a device has all of the URL patterns allowed to access it.
  const auto& url_patterns = map.at(device_key);
  EXPECT_TRUE(base::ContainsKey(
      url_patterns,
      content_settings::ParsePatternString("https://[*.]google.com")));
  EXPECT_TRUE(base::ContainsKey(
      url_patterns, content_settings::ParsePatternString("https://crbug.com")));
  EXPECT_TRUE(base::ContainsKey(
      url_patterns,
      content_settings::ParsePatternString("https://[*.]youtube.com")));
}

namespace {

constexpr char kPolicySettingWithEntriesMatchingMultipleDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "url_patterns": ["https://[*.]google.com"]
      }, {
        "devices": [{ "vendor_id": 1234 }],
        "url_patterns": ["https://[*.]youtube.com"]
      }, {
        "devices": [{}],
        "url_patterns": ["https://[*.]chromium.org"]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowed) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const GURL origins_for_specific_device[] = {GURL("https://google.com"),
                                              GURL("https://mail.google.com")};
  const GURL origins_for_specific_vendor_devices[] = {
      GURL("https://youtube.com"), GURL("https://music.youtube.com")};
  const GURL origins_for_any_device[] = {GURL("https://chromium.org"),
                                         GURL("https://bugs.chromium.org")};

  scoped_refptr<device::UsbDevice> specific_device =
      base::MakeRefCounted<device::MockUsbDevice>(1234, 5678, "Google", "Gizmo",
                                                  "123ABC");
  scoped_refptr<device::UsbDevice> vendor_device =
      base::MakeRefCounted<device::MockUsbDevice>(1234, 8765, "Google", "Gizmo",
                                                  "ABC123");
  scoped_refptr<device::UsbDevice> unrelated_device =
      base::MakeRefCounted<device::MockUsbDevice>(4321, 8765, "Chrome", "Gizmo",
                                                  "987ZYX");

  auto specific_device_info =
      device::mojom::UsbDeviceInfo::From(*specific_device);
  auto vendor_device_info = device::mojom::UsbDeviceInfo::From(*vendor_device);
  auto unrelated_device_info =
      device::mojom::UsbDeviceInfo::From(*unrelated_device);

  // Check the URLs for the specific device.
  for (const GURL& requesting_origin : origins_for_specific_device) {
    for (const GURL& embedding_origin : origins_for_specific_device) {
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *specific_device_info));
    }
  }

  // Check the URLs for vendor devices.
  for (const GURL& requesting_origin : origins_for_specific_vendor_devices) {
    for (const GURL& embedding_origin : origins_for_specific_vendor_devices) {
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *specific_device_info));
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *vendor_device_info));
    }
  }

  // Check the URLs for any device.
  for (const GURL& requesting_origin : origins_for_any_device) {
    for (const GURL& embedding_origin : origins_for_any_device) {
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *specific_device_info));
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *vendor_device_info));
      EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *unrelated_device_info));
    }
  }
}

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForUrlPatternsNotInPref) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const GURL origins[] = {GURL("https://evil.com"),
                          GURL("https://very.evil.com"),
                          GURL("https://chromium.deceptive.org")};

  scoped_refptr<device::UsbDevice> device =
      base::MakeRefCounted<device::MockUsbDevice>(1234, 5678, "Google", "Gizmo",
                                                  "123ABC");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  for (const GURL& requesting_origin : origins) {
    for (const GURL& embedding_origin : origins) {
      EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *device_info));
    }
  }
}

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForDeviceNotInPref) {
  std::unique_ptr<base::Value> pref_value =
      base::JSONReader::Read(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const GURL origins[] = {
      GURL("https://google.com"), GURL("https://mail.google.com"),
      GURL("https://youtube.com"), GURL("https://music.youtube.com")};

  scoped_refptr<device::UsbDevice> device =
      base::MakeRefCounted<device::MockUsbDevice>(4321, 8765, "Google", "Gizmo",
                                                  "123ABC");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  for (const GURL& requesting_origin : origins) {
    for (const GURL& embedding_origin : origins) {
      EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
          requesting_origin, embedding_origin, *device_info));
    }
  }
}

namespace {

constexpr char kPolicySettingWithUrlPatternContainingEmbeddingOrigin[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "url_patterns": [
          "https://[*.]requesting.com,https://[*.]embedding.com"
        ]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest,
       IsDeviceAllowedForUrlPatternContainingEmbeddingOrigin) {
  std::unique_ptr<base::Value> pref_value = base::JSONReader::Read(
      kPolicySettingWithUrlPatternContainingEmbeddingOrigin);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices =
      std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());

  const GURL requesting_origin("https://requesting.com");
  const GURL embedding_origin("https://embedding.com");

  scoped_refptr<device::UsbDevice> device =
      base::MakeRefCounted<device::MockUsbDevice>(1234, 5678, "Google", "Gizmo",
                                                  "123ABC");
  auto device_info = device::mojom::UsbDeviceInfo::From(*device);
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      requesting_origin, embedding_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      embedding_origin, requesting_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      requesting_origin, requesting_origin, *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      embedding_origin, embedding_origin, *device_info));
}
