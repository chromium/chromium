// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/usb/usb_policy_allowed_devices.h"

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

class UsbPolicyAllowedDevicesTest : public testing::Test {
 public:
  UsbPolicyAllowedDevicesTest() = default;
  ~UsbPolicyAllowedDevicesTest() override = default;

  void SetWebUsbAllowDevicesForUrlsPrefValue(const base::Value& value) {
    profile_.GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls, value);
  }

 protected:
  Profile* profile() { return &profile_; }

  std::unique_ptr<UsbPolicyAllowedDevices> CreateUsbPolicyAllowedDevices() {
    return std::make_unique<UsbPolicyAllowedDevices>(profile()->GetPrefs());
  }

  device::FakeUsbDeviceManager device_manager_;

  const url::Origin kGoogleOrigin =
      url::Origin::Create(GURL("https://google.com"));
  const url::Origin kCrbugOrigin =
      url::Origin::Create(GURL("https://crbug.com"));
  const url::Origin kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

std::unique_ptr<base::Value> ReadJson(base::StringPiece json) {
  base::Optional<base::Value> value = base::JSONReader::Read(json);
  EXPECT_TRUE(value);
  return value ? base::Value::ToUniquePtrValue(std::move(*value)) : nullptr;
}

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithMissingPrefValue) {
  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithExistingEmptyPrefValue) {
  base::Value pref_value(base::Value::Type::LIST);

  SetWebUsbAllowDevicesForUrlsPrefValue(pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

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
        "urls": [
          "https://google.com,https://google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{}],
        "urls": ["https://www.youtube.com"]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, InitializeWithExistingPrefValue) {
  std::unique_ptr<base::Value> pref_value = ReadJson(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(first_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(first_urls, kCrbugOrigin));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(second_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(second_urls, kCrbugOrigin));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(third_urls, kYoutubeOrigin));
}

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithMissingPolicyThenUpdatePolicy) {
  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();
  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());

  // Ensure that the allowed devices can be dynamically updated.
  std::unique_ptr<base::Value> pref_value = ReadJson(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(first_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(first_urls, kCrbugOrigin));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(second_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(second_urls, kCrbugOrigin));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(third_urls, kYoutubeOrigin));
}

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPolicyThenRemovePolicy) {
  std::unique_ptr<base::Value> pref_value = ReadJson(kPolicySetting);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  EXPECT_EQ(map.size(), 3ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& first_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(first_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(first_urls, kCrbugOrigin));

  device_key = std::make_pair(4321, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& second_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(second_urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(second_urls, kCrbugOrigin));

  device_key = std::make_pair(-1, -1);
  ASSERT_TRUE(base::Contains(map, device_key));

  const auto& third_urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(third_urls, kYoutubeOrigin));

  // Ensure that the allowed devices can be removed dynamically.
  pref_value = std::make_unique<base::Value>(base::Value::Type::LIST);
  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  EXPECT_TRUE(usb_policy_allowed_devices->map().empty());
}

namespace {

constexpr char kPolicySettingWithEntriesContainingDuplicateDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [
          "https://google.com",
          "https://crbug.com"
        ]
      }, {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://www.youtube.com"]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest,
       InitializeWithExistingPrefValueContainingDuplicateDevices) {
  std::unique_ptr<base::Value> pref_value =
      ReadJson(kPolicySettingWithEntriesContainingDuplicateDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const UsbPolicyAllowedDevices::UsbDeviceIdsToUrlsMap& map =
      usb_policy_allowed_devices->map();
  ASSERT_EQ(map.size(), 1ul);

  auto device_key = std::make_pair(1234, 5678);
  ASSERT_TRUE(base::Contains(map, device_key));

  // Ensure a device has all of the URL patterns allowed to access it.
  const auto& urls = map.at(device_key);
  EXPECT_TRUE(base::Contains(urls, kGoogleOrigin));
  EXPECT_TRUE(base::Contains(urls, kCrbugOrigin));
  EXPECT_TRUE(base::Contains(urls, kYoutubeOrigin));
}

namespace {

constexpr char kPolicySettingWithEntriesMatchingMultipleDevices[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": ["https://google.com"]
      }, {
        "devices": [{ "vendor_id": 1234 }],
        "urls": ["https://www.youtube.com"]
      }, {
        "devices": [{}],
        "urls": ["https://chromium.org"]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowed) {
  std::unique_ptr<base::Value> pref_value =
      ReadJson(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const auto kGoogleOrigin = url::Origin::Create(GURL("https://google.com"));
  const auto kYoutubeOrigin =
      url::Origin::Create(GURL("https://www.youtube.com"));
  const auto kChromiumOrigin =
      url::Origin::Create(GURL("https://chromium.org"));

  auto specific_device_info = device_manager_.CreateAndAddDevice(
      1234, 5678, "Google", "Gizmo", "123ABC");
  auto vendor_device_info = device_manager_.CreateAndAddDevice(
      1234, 8765, "Google", "Gizmo", "ABC123");
  auto unrelated_device_info = device_manager_.CreateAndAddDevice(
      4321, 8765, "Chrome", "Gizmo", "987ZYX");

  // Check that the specific device is allowed for https://google.com but not
  // any other device.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, *specific_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, *vendor_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kGoogleOrigin, *unrelated_device_info));

  // Check that devices with a vendor ID of 1234 are allowed for
  // https://www.youtube.com, but not an unrelated device.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(kYoutubeOrigin,
                                                          *vendor_device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(
      kYoutubeOrigin, *unrelated_device_info));

  // Check that any device is allowed for https://chromium.org.
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, *specific_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(kChromiumOrigin,
                                                          *vendor_device_info));
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(
      kChromiumOrigin, *unrelated_device_info));
}

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForUrlsNotInPref) {
  std::unique_ptr<base::Value> pref_value =
      ReadJson(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const url::Origin origins[] = {
      url::Origin::Create(GURL("https://evil.com")),
      url::Origin::Create(GURL("https://very.evil.com")),
      url::Origin::Create(GURL("https://chromium.deceptive.org"))};

  auto device_info = device_manager_.CreateAndAddDevice(1234, 5678, "Google",
                                                        "Gizmo", "123ABC");
  for (const url::Origin& origin : origins) {
    EXPECT_FALSE(
        usb_policy_allowed_devices->IsDeviceAllowed(origin, *device_info));
  }
}

TEST_F(UsbPolicyAllowedDevicesTest, IsDeviceAllowedForDeviceNotInPref) {
  std::unique_ptr<base::Value> pref_value =
      ReadJson(kPolicySettingWithEntriesMatchingMultipleDevices);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const url::Origin origins[] = {
      url::Origin::Create(GURL("https://google.com")),
      url::Origin::Create(GURL("https://www.youtube.com"))};

  auto device_info = device_manager_.CreateAndAddDevice(4321, 8765, "Google",
                                                        "Gizmo", "123ABC");
  for (const url::Origin& origin : origins) {
    EXPECT_FALSE(
        usb_policy_allowed_devices->IsDeviceAllowed(origin, *device_info));
  }
}

namespace {

constexpr char kPolicySettingWithUrlContainingEmbeddingOrigin[] = R"(
    [
      {
        "devices": [{ "vendor_id": 1234, "product_id": 5678 }],
        "urls": [
          "https://requesting.com,https://embedding.com"
        ]
      }
    ])";

}  // namespace

TEST_F(UsbPolicyAllowedDevicesTest,
       IsDeviceAllowedForUrlContainingEmbeddingOrigin) {
  std::unique_ptr<base::Value> pref_value =
      ReadJson(kPolicySettingWithUrlContainingEmbeddingOrigin);

  SetWebUsbAllowDevicesForUrlsPrefValue(*pref_value);

  auto usb_policy_allowed_devices = CreateUsbPolicyAllowedDevices();

  const auto requesting_origin =
      url::Origin::Create(GURL("https://requesting.com"));
  const auto embedding_origin =
      url::Origin::Create(GURL("https://embedding.com"));

  auto device_info = device_manager_.CreateAndAddDevice(1234, 5678, "Google",
                                                        "Gizmo", "123ABC");
  EXPECT_TRUE(usb_policy_allowed_devices->IsDeviceAllowed(embedding_origin,
                                                          *device_info));
  EXPECT_FALSE(usb_policy_allowed_devices->IsDeviceAllowed(requesting_origin,
                                                           *device_info));
}
