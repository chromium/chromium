// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/usb/usb_chooser_context.h"
#include "chrome/browser/usb/usb_chooser_context_factory.h"
#include "chrome/browser/usb/usb_chooser_context_mock_device_observer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using device::mojom::UsbDeviceInfoPtr;
using ::testing::_;
using ::testing::AnyNumber;

namespace {

constexpr char kDeviceNameKey[] = "name";
constexpr char kGuidKey[] = "ephemeral-guid";
constexpr char kProductIdKey[] = "product-id";
constexpr char kSerialNumberKey[] = "serial-number";
constexpr char kVendorIdKey[] = "vendor-id";
constexpr int kDeviceIdWildcard = -1;

class UsbChooserContextTest : public testing::Test {
 public:
  UsbChooserContextTest() {}
  ~UsbChooserContextTest() override {
    // When UsbChooserContext is destroyed, OnDeviceManagerConnectionError
    // should be called on the observers and OnPermissionRevoked will be called
    // for any ephemeral device permissions that are active.
    EXPECT_CALL(mock_device_observer_, OnDeviceManagerConnectionError())
        .Times(AnyNumber());
    EXPECT_CALL(mock_permission_observer_, OnPermissionRevoked(_))
        .Times(AnyNumber());
    EXPECT_CALL(mock_permission_observer_,
                OnChooserObjectPermissionChanged(_, _))
        .Times(AnyNumber());
  }

 protected:
  TestingProfile* profile() { return &profile_; }

  UsbChooserContext* GetChooserContext(Profile* profile) {
    auto* chooser_context = UsbChooserContextFactory::GetForProfile(profile);
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    chooser_context->SetDeviceManagerForTesting(std::move(device_manager));

    // Call GetDevices once to make sure the connection with DeviceManager has
    // been set up, so that it can be notified when device is removed.
    chooser_context->GetDevices(
        base::DoNothing::Once<std::vector<UsbDeviceInfoPtr>>());
    base::RunLoop().RunUntilIdle();

    // Add observers
    chooser_context->permissions::ChooserContextBase::AddObserver(
        &mock_permission_observer_);
    chooser_context->AddObserver(&mock_device_observer_);
    return chooser_context;
  }

  device::FakeUsbDeviceManager device_manager_;

  // Mock observers
  permissions::MockPermissionObserver mock_permission_observer_;
  MockDeviceObserver mock_device_observer_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

}  // namespace

TEST_F(UsbChooserContextTest, CheckGrantAndRevokePermission) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");
  UsbChooserContext* store = GetChooserContext(profile());

  base::Value object(base::Value::Type::DICTIONARY);
  object.SetStringKey(kDeviceNameKey, "Gizmo");
  object.SetIntKey(kVendorIdKey, 0);
  object.SetIntKey(kProductIdKey, 0);
  object.SetStringKey(kSerialNumberKey, "123ABC");

  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  store->GrantDevicePermission(origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info));
  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(origin);
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(object, objects[0]->value);

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  ASSERT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(url, all_origin_objects[0]->origin);
  EXPECT_EQ(object, all_origin_objects[0]->value);
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(mock_permission_observer_, OnPermissionRevoked(origin));

  store->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));

  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, CheckGrantAndRevokeEphemeralPermission) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbDeviceInfoPtr other_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  UsbChooserContext* store = GetChooserContext(profile());

  base::Value object(base::Value::Type::DICTIONARY);
  object.SetStringKey(kDeviceNameKey, "Gizmo");
  object.SetStringKey(kGuidKey, device_info->guid);
  object.SetIntKey(kVendorIdKey, device_info->vendor_id);
  object.SetIntKey(kProductIdKey, device_info->product_id);

  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  store->GrantDevicePermission(origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(origin, *other_device_info));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object, objects[0]->value);

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
  EXPECT_EQ(url, all_origin_objects[0]->origin);
  EXPECT_EQ(object, all_origin_objects[0]->value);
  EXPECT_FALSE(all_origin_objects[0]->incognito);

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(mock_permission_observer_, OnPermissionRevoked(origin));

  store->RevokeObjectPermission(origin, objects[0]->value);
  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));

  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithPermission) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");

  UsbChooserContext* store = GetChooserContext(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  store->GrantDevicePermission(origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  EXPECT_CALL(mock_device_observer_, OnDeviceRemoved(_));
  device_manager_.RemoveDevice(device_info->guid);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info));
  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  UsbDeviceInfoPtr reconnected_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "123ABC");

  EXPECT_TRUE(store->HasDevicePermission(origin, *reconnected_device_info));
  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, DisconnectDeviceWithEphemeralPermission) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  UsbChooserContext* store = GetChooserContext(profile());

  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  store->GrantDevicePermission(origin, *device_info);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  EXPECT_CALL(mock_device_observer_, OnDeviceRemoved(_));
  device_manager_.RemoveDevice(device_info->guid);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info));
  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());

  UsbDeviceInfoPtr reconnected_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  EXPECT_FALSE(store->HasDevicePermission(origin, *reconnected_device_info));
  objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());
  all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, GrantPermissionInIncognito) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);
  UsbDeviceInfoPtr device_info_1 =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbDeviceInfoPtr device_info_2 =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");
  UsbChooserContext* store = GetChooserContext(profile());
  UsbChooserContext* incognito_store =
      GetChooserContext(profile()->GetPrimaryOTRProfile());

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  store->GrantDevicePermission(origin, *device_info_1);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info_1));
  EXPECT_FALSE(incognito_store->HasDevicePermission(origin, *device_info_1));

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));

  incognito_store->GrantDevicePermission(origin, *device_info_2);
  EXPECT_TRUE(store->HasDevicePermission(origin, *device_info_1));
  EXPECT_FALSE(store->HasDevicePermission(origin, *device_info_2));
  EXPECT_FALSE(incognito_store->HasDevicePermission(origin, *device_info_1));
  EXPECT_TRUE(incognito_store->HasDevicePermission(origin, *device_info_2));

  {
    std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
        objects = store->GetGrantedObjects(origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
        all_origin_objects = store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_FALSE(all_origin_objects[0]->incognito);
  }
  {
    std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
        objects = incognito_store->GetGrantedObjects(origin);
    EXPECT_EQ(1u, objects.size());
    std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
        all_origin_objects = incognito_store->GetAllGrantedObjects();
    ASSERT_EQ(1u, all_origin_objects.size());
    EXPECT_TRUE(all_origin_objects[0]->incognito);
  }
}

TEST_F(UsbChooserContextTest, UsbGuardPermission) {
  const GURL kFooUrl("https://foo.com");
  const auto kFooOrigin = url::Origin::Create(kFooUrl);
  const GURL kBarUrl("https://bar.com");
  const auto kBarOrigin = url::Origin::Create(kBarUrl);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "ABC123");
  UsbDeviceInfoPtr ephemeral_device_info =
      device_manager_.CreateAndAddDevice(0, 0, "Google", "Gizmo", "");

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(
      kFooUrl, kFooUrl, ContentSettingsType::USB_GUARD, CONTENT_SETTING_BLOCK);

  auto* store = GetChooserContext(profile());
  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA))
      .Times(4);
  store->GrantDevicePermission(kFooOrigin, *device_info);
  store->GrantDevicePermission(kFooOrigin, *ephemeral_device_info);
  store->GrantDevicePermission(kBarOrigin, *device_info);
  store->GrantDevicePermission(kBarOrigin, *ephemeral_device_info);

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(0u, objects.size());

  objects = store->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(2u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  for (const auto& object : all_origin_objects) {
    EXPECT_EQ(object->origin, kBarUrl);
  }
  EXPECT_EQ(2u, all_origin_objects.size());

  EXPECT_FALSE(store->HasDevicePermission(kFooOrigin, *device_info));
  EXPECT_FALSE(store->HasDevicePermission(kFooOrigin, *ephemeral_device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, *device_info));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, *ephemeral_device_info));
}

TEST_F(UsbChooserContextTest, GetObjectDisplayNameForNamelessDevice) {
  const GURL kGoogleUrl("https://www.google.com");
  const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
  UsbDeviceInfoPtr device_info =
      device_manager_.CreateAndAddDevice(6353, 5678, "", "", "");

  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(kGoogleOrigin, *device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 1u);
  EXPECT_EQ(store->GetObjectDisplayName(objects[0]->value),
            u"Unknown product 0x162E from Google Inc.");
}

TEST_F(UsbChooserContextTest, PolicyGuardPermission) {
  const auto origin = url::Origin::Create(GURL("https://google.com"));

  UsbDeviceInfoPtr device =
      device_manager_.CreateAndAddDevice(0, 0, "", "", "");
  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(origin, *device);

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultWebUsbGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  EXPECT_FALSE(store->CanRequestObjectPermission(origin));
  EXPECT_FALSE(store->HasDevicePermission(origin, *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(origin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(0u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, PolicyAskForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  UsbDeviceInfoPtr device =
      device_manager_.CreateAndAddDevice(0, 0, "", "", "");
  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(kFooOrigin, *device);
  store->GrantDevicePermission(kBarOrigin, *device);

  // Set the default to "ask" so that the policy being tested overrides it.
  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedDefaultWebUsbGuardSetting,
                        std::make_unique<base::Value>(CONTENT_SETTING_BLOCK));
  prefs->SetManagedPref(prefs::kManagedWebUsbAskForUrls,
                        base::JSONReader::ReadDeprecated(R"(
    [ "https://foo.origin" ]
  )"));

  EXPECT_TRUE(store->CanRequestObjectPermission(kFooOrigin));
  EXPECT_TRUE(store->HasDevicePermission(kFooOrigin, *device));
  EXPECT_FALSE(store->CanRequestObjectPermission(kBarOrigin));
  EXPECT_FALSE(store->HasDevicePermission(kBarOrigin, *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(1u, objects.size());
  objects = store->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(0u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

TEST_F(UsbChooserContextTest, PolicyBlockedForUrls) {
  const auto kFooOrigin = url::Origin::Create(GURL("https://foo.origin"));
  const auto kBarOrigin = url::Origin::Create(GURL("https://bar.origin"));

  UsbDeviceInfoPtr device =
      device_manager_.CreateAndAddDevice(0, 0, "", "", "");
  auto* store = GetChooserContext(profile());
  store->GrantDevicePermission(kFooOrigin, *device);
  store->GrantDevicePermission(kBarOrigin, *device);

  auto* prefs = profile()->GetTestingPrefService();
  prefs->SetManagedPref(prefs::kManagedWebUsbBlockedForUrls,
                        base::JSONReader::ReadDeprecated(R"(
    [ "https://foo.origin" ]
  )"));

  EXPECT_FALSE(store->CanRequestObjectPermission(kFooOrigin));
  EXPECT_FALSE(store->HasDevicePermission(kFooOrigin, *device));
  EXPECT_TRUE(store->CanRequestObjectPermission(kBarOrigin));
  EXPECT_TRUE(store->HasDevicePermission(kBarOrigin, *device));

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      objects = store->GetGrantedObjects(kFooOrigin);
  EXPECT_EQ(0u, objects.size());
  objects = store->GetGrantedObjects(kBarOrigin);
  EXPECT_EQ(1u, objects.size());

  std::vector<std::unique_ptr<permissions::ChooserContextBase::Object>>
      all_origin_objects = store->GetAllGrantedObjects();
  EXPECT_EQ(1u, all_origin_objects.size());
}

namespace {

constexpr char kPolicySetting[] = R"(
    [
      {
        "devices": [{ "vendor_id": 6353, "product_id": 5678 }],
        "urls": ["https://product.vendor.com"]
      }, {
        "devices": [{ "vendor_id": 6353 }],
        "urls": ["https://vendor.com"]
      }, {
        "devices": [{}],
        "urls": ["https://anydevice.com"]
      }, {
        "devices": [{ "vendor_id": 6354, "product_id": 1357 }],
        "urls": ["https://gadget.com,https://cool.com"]
      }
    ])";

// Test URLs
// TODO(https://crbug.com/1042727): Fix test GURL scoping and remove this getter
// function.
GURL AnyDeviceUrl() {
  return GURL("https://anydevice.com");
}
GURL VendorUrl() {
  return GURL("https://vendor.com");
}
GURL ProductVendorUrl() {
  return GURL("https://product.vendor.com");
}
GURL GadgetUrl() {
  return GURL("https://gadget.com");
}
GURL CoolUrl() {
  return GURL("https://cool.com");
}

const std::vector<GURL>& PolicyOrigins() {
  static base::NoDestructor<std::vector<GURL>> origins{
      {ProductVendorUrl(), VendorUrl(), AnyDeviceUrl(), GadgetUrl(),
       CoolUrl()}};
  return *origins;
}

void ExpectNoPermissions(UsbChooserContext* store,
                         const device::mojom::UsbDeviceInfo& device_info) {
  for (const auto& origin_url : PolicyOrigins()) {
    const auto origin = url::Origin::Create(origin_url);
    EXPECT_FALSE(store->HasDevicePermission(origin, device_info));
  }
}

void ExpectCorrectPermissions(UsbChooserContext* store,
                              const std::vector<GURL>& kValidOrigins,
                              const std::vector<GURL>& kInvalidOrigins,
                              const device::mojom::UsbDeviceInfo& device_info) {
  // Ensure that only |kValidOrigin| as the top-level origin has
  // permission to access the device described by |device_info|.
  for (const auto& valid_origin : kValidOrigins) {
    EXPECT_TRUE(store->HasDevicePermission(url::Origin::Create(valid_origin),
                                           device_info));
  }

  for (const auto& invalid_origin : kInvalidOrigins) {
    EXPECT_FALSE(store->HasDevicePermission(url::Origin::Create(invalid_origin),
                                            device_info));
  }
}

}  // namespace

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForSpecificDevice) {
  const std::vector<GURL> kValidOrigins = {ProductVendorUrl(), VendorUrl(),
                                           AnyDeviceUrl()};
  const std::vector<GURL> kInvalidOrigins = {GadgetUrl(), CoolUrl()};

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *specific_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  ExpectCorrectPermissions(store, kValidOrigins, kInvalidOrigins,
                           *specific_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForVendorRelatedDevice) {
  const std::vector<GURL> kValidOrigins = {VendorUrl(), AnyDeviceUrl()};
  const std::vector<GURL> kInvalidOrigins = {ProductVendorUrl(), GadgetUrl(),
                                             CoolUrl()};

  UsbDeviceInfoPtr vendor_related_device_info =
      device_manager_.CreateAndAddDevice(6353, 8765, "Google", "Widget",
                                         "XYZ987");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *vendor_related_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  ExpectCorrectPermissions(store, kValidOrigins, kInvalidOrigins,
                           *vendor_related_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionForUnrelatedDevice) {
  const std::vector<GURL> kValidOrigins = {AnyDeviceUrl()};
  const std::vector<GURL> kInvalidOrigins = {ProductVendorUrl(), VendorUrl(),
                                             GadgetUrl()};
  const auto kCoolOrigin = url::Origin::Create(CoolUrl());

  UsbDeviceInfoPtr unrelated_device_info = device_manager_.CreateAndAddDevice(
      6354, 1357, "Cool", "Gadget", "4W350M3");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *unrelated_device_info);

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  EXPECT_TRUE(store->HasDevicePermission(kCoolOrigin, *unrelated_device_info));
  for (const auto& origin_url : PolicyOrigins()) {
    const auto origin = url::Origin::Create(origin_url);
    if (origin_url != CoolUrl() && origin_url != AnyDeviceUrl()) {
      EXPECT_FALSE(store->HasDevicePermission(origin, *unrelated_device_info));
    }
  }
  ExpectCorrectPermissions(store, kValidOrigins, kInvalidOrigins,
                           *unrelated_device_info);
}

TEST_F(UsbChooserContextTest,
       UsbAllowDevicesForUrlsPermissionOverrulesUsbGuardPermission) {
  const auto kProductVendorOrigin = url::Origin::Create(ProductVendorUrl());
  const auto kGadgetOrigin = url::Origin::Create(GadgetUrl());
  const auto kCoolOrigin = url::Origin::Create(CoolUrl());

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");
  UsbDeviceInfoPtr unrelated_device_info = device_manager_.CreateAndAddDevice(
      6354, 1357, "Cool", "Gadget", "4W350M3");

  auto* store = GetChooserContext(profile());

  ExpectNoPermissions(store, *specific_device_info);
  ExpectNoPermissions(store, *unrelated_device_info);

  auto* map = HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetContentSettingDefaultScope(ProductVendorUrl(), ProductVendorUrl(),
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);
  map->SetContentSettingDefaultScope(GadgetUrl(), CoolUrl(),
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(
      store->HasDevicePermission(kProductVendorOrigin, *specific_device_info));
  EXPECT_FALSE(
      store->HasDevicePermission(kProductVendorOrigin, *unrelated_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kCoolOrigin, *specific_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kCoolOrigin, *unrelated_device_info));

  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  EXPECT_TRUE(
      store->HasDevicePermission(kProductVendorOrigin, *specific_device_info));
  EXPECT_FALSE(
      store->HasDevicePermission(kProductVendorOrigin, *unrelated_device_info));
  EXPECT_FALSE(store->HasDevicePermission(kCoolOrigin, *specific_device_info));
  EXPECT_TRUE(store->HasDevicePermission(kCoolOrigin, *unrelated_device_info));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)

class DeviceLoginScreenWebUsbChooserContextTest : public UsbChooserContextTest {
 public:
  DeviceLoginScreenWebUsbChooserContextTest() {
    TestingProfile::Builder builder;
    builder.SetPath(base::FilePath(FILE_PATH_LITERAL(chrome::kInitialProfile)));
    signin_profile_ = builder.Build();
  }
  ~DeviceLoginScreenWebUsbChooserContextTest() override {}

 protected:
  Profile* GetSigninProfile() { return signin_profile_.get(); }

 private:
  std::unique_ptr<Profile> signin_profile_;
};

TEST_F(DeviceLoginScreenWebUsbChooserContextTest,
       UserUsbChooserContextOnlyUsesUserPolicy) {
  const std::vector<GURL> kValidOrigins = {ProductVendorUrl(), VendorUrl(),
                                           AnyDeviceUrl()};
  const std::vector<GURL> kInvalidOrigins = {GadgetUrl(), CoolUrl()};

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");

  Profile* user_profile = profile();
  Profile* signin_profile = GetSigninProfile();

  auto* user_store = GetChooserContext(user_profile);
  auto* signin_store = GetChooserContext(signin_profile);

  ExpectNoPermissions(user_store, *specific_device_info);
  ExpectNoPermissions(signin_store, *specific_device_info);

  user_profile->GetPrefs()->Set(
      prefs::kManagedWebUsbAllowDevicesForUrls,
      *base::JSONReader::ReadDeprecated(kPolicySetting));

  ExpectCorrectPermissions(user_store, kValidOrigins, kInvalidOrigins,
                           *specific_device_info);
  ExpectNoPermissions(signin_store, *specific_device_info);
}

TEST_F(DeviceLoginScreenWebUsbChooserContextTest,
       SigninUsbChooserContextOnlyUsesDevicePolicy) {
  const std::vector<GURL> kValidOrigins = {ProductVendorUrl(), VendorUrl(),
                                           AnyDeviceUrl()};
  const std::vector<GURL> kInvalidOrigins = {GadgetUrl(), CoolUrl()};

  UsbDeviceInfoPtr specific_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "ABC123");

  Profile* user_profile = profile();
  Profile* signin_profile = GetSigninProfile();

  auto* user_store = GetChooserContext(user_profile);
  auto* signin_store = GetChooserContext(signin_profile);

  ExpectNoPermissions(user_store, *specific_device_info);
  ExpectNoPermissions(signin_store, *specific_device_info);

  signin_profile->GetPrefs()->Set(
      prefs::kManagedWebUsbAllowDevicesForUrls,
      *base::JSONReader::ReadDeprecated(kPolicySetting));

  ExpectNoPermissions(user_store, *specific_device_info);
  ExpectCorrectPermissions(signin_store, kValidOrigins, kInvalidOrigins,
                           *specific_device_info);
}

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

void ExpectDeviceObjectInfo(const base::Value& actual,
                            int vendor_id,
                            int product_id,
                            const std::string& name) {
  const base::Optional<int> actual_vendor_id = actual.FindIntKey(kVendorIdKey);
  ASSERT_TRUE(actual_vendor_id);
  EXPECT_EQ(*actual_vendor_id, vendor_id);

  const base::Optional<int> actual_product_id =
      actual.FindIntKey(kProductIdKey);
  ASSERT_TRUE(actual_product_id);
  EXPECT_EQ(*actual_product_id, product_id);

  const std::string* actual_device_name = actual.FindStringKey(kDeviceNameKey);
  ASSERT_TRUE(actual_device_name);
  EXPECT_EQ(*actual_device_name, name);
}

void ExpectChooserObjectInfo(
    const permissions::ChooserContextBase::Object* actual,
    const GURL& origin,
    content_settings::SettingSource source,
    bool incognito,
    int vendor_id,
    int product_id,
    const std::string& name) {
  ASSERT_TRUE(actual);
  EXPECT_EQ(actual->origin, origin);
  EXPECT_EQ(actual->source, source);
  EXPECT_EQ(actual->incognito, incognito);
  ExpectDeviceObjectInfo(actual->value, vendor_id, product_id, name);
}

}  // namespace

TEST_F(UsbChooserContextTest, GetGrantedObjectsWithOnlyPolicyAllowedDevices) {
  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  const auto kVendorOrigin = url::Origin::Create(VendorUrl());
  auto objects = store->GetGrantedObjects(kVendorOrigin);
  ASSERT_EQ(objects.size(), 1u);

  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
}

TEST_F(UsbChooserContextTest,
       GetGrantedObjectsWithUserAndPolicyAllowedDevices) {
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  UsbDeviceInfoPtr persistent_device_info =
      device_manager_.CreateAndAddDevice(1000, 1, "Google", "Gizmo", "123ABC");
  UsbDeviceInfoPtr ephemeral_device_info =
      device_manager_.CreateAndAddDevice(1000, 2, "Google", "Gadget", "");

  auto* store = GetChooserContext(profile());

  const auto kVendorOrigin = url::Origin::Create(VendorUrl());
  store->GrantDevicePermission(kVendorOrigin, *persistent_device_info);
  store->GrantDevicePermission(kVendorOrigin, *ephemeral_device_info);

  auto objects = store->GetGrantedObjects(kVendorOrigin);
  ASSERT_EQ(objects.size(), 3u);

  // The user granted permissions appear before the policy granted permissions.
  // Within these user granted permissions, the persistent device permissions
  // appear before ephemeral device permissions. The policy enforced objects
  // that are returned by GetGrantedObjects() are ordered by the tuple
  // (vendor_id, product_id) representing the device IDs.  Wildcard IDs are
  // represented by a value of -1, so they appear first.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_USER,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/1,
                          /*name=*/"Gizmo");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_USER,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/2,
                          /*name=*/"Gadget");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
}

TEST_F(UsbChooserContextTest,
       GetGrantedObjectsWithUserGrantedDeviceAllowedBySpecificDevicePolicy) {
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Google", "Gizmo", "123ABC");

  auto* store = GetChooserContext(profile());
  const auto kProductVendorOrigin = url::Origin::Create(ProductVendorUrl());
  store->GrantDevicePermission(kProductVendorOrigin, *persistent_device_info);

  auto objects = store->GetGrantedObjects(kProductVendorOrigin);
  ASSERT_EQ(objects.size(), 1u);

  // User granted permissions for a device that is also granted by a specific
  // device policy will be replaced by the policy permission. The object should
  // still retain the name of the device.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Gizmo");
}

TEST_F(UsbChooserContextTest,
       GetGrantedObjectsWithUserGrantedDeviceAllowedByVendorDevicePolicy) {
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 1000, "Vendor", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  const auto kVendorOrigin = url::Origin::Create(VendorUrl());
  store->GrantDevicePermission(kVendorOrigin, *persistent_device_info);

  auto objects = store->GetGrantedObjects(kVendorOrigin);
  ASSERT_EQ(objects.size(), 1u);

  // User granted permissions for a device that is also granted by a vendor
  // device policy will be replaced by the policy permission.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
}

TEST_F(UsbChooserContextTest,
       GetGrantedObjectsWithUserGrantedDeviceAllowedByAnyVendorDevicePolicy) {
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      1123, 5813, "Some", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  const auto kAnyDeviceOrigin = url::Origin::Create(AnyDeviceUrl());
  store->GrantDevicePermission(kAnyDeviceOrigin, *persistent_device_info);

  auto objects = store->GetGrantedObjects(kAnyDeviceOrigin);
  ASSERT_EQ(objects.size(), 1u);

  // User granted permissions for a device that is also granted by a wildcard
  // vendor policy will be replaced by the policy permission.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithOnlyPolicyAllowedDevices) {
  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4u);

  // The policy enforced objects that are returned by GetAllGrantedObjects() are
  // ordered by the tuple (vendor_id, product_id) representing the device IDs.
  // Wildcard IDs are represented by a value of -1, so they appear first.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown product 0x162E from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*origin=*/CoolUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown product 0x054D from vendor 0x18D2");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithUserAndPolicyAllowedDevices) {
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  const GURL kGoogleUrl("https://www.google.com");
  const auto kGoogleOrigin = url::Origin::Create(kGoogleUrl);
  UsbDeviceInfoPtr persistent_device_info =
      device_manager_.CreateAndAddDevice(1000, 1, "Google", "Gizmo", "123ABC");
  UsbDeviceInfoPtr ephemeral_device_info =
      device_manager_.CreateAndAddDevice(1000, 2, "Google", "Gadget", "");

  auto* store = GetChooserContext(profile());

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA))
      .Times(2);
  store->GrantDevicePermission(kGoogleOrigin, *persistent_device_info);
  store->GrantDevicePermission(kGoogleOrigin, *ephemeral_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 6u);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->value));
  }

  // The user granted permissions appear before the policy granted permissions.
  // Within the user granted permissions, the persistent device permissions
  // are added to the vector before ephemeral device permissions.
  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/kGoogleUrl,
                          /*source=*/content_settings::SETTING_SOURCE_USER,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/1,
                          /*name=*/"Gizmo");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/kGoogleUrl,
                          /*source=*/content_settings::SETTING_SOURCE_USER,
                          /*incognito=*/false,
                          /*vendor_id=*/1000,
                          /*product_id=*/2,
                          /*name=*/"Gadget");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
  ExpectChooserObjectInfo(objects[4].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown product 0x162E from Google Inc.");
  ExpectChooserObjectInfo(objects[5].get(),
                          /*origin=*/CoolUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown product 0x054D from vendor 0x18D2");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithSpecificPolicyAndUserGrantedDevice) {
  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 5678, "Specific", "Product", "123ABC");

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  const auto kProductVendorOrigin = url::Origin::Create(ProductVendorUrl());
  store->GrantDevicePermission(kProductVendorOrigin, *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4u);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->value));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Product");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*origin=*/CoolUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown product 0x054D from vendor 0x18D2");
  ASSERT_TRUE(persistent_device_info->product_name);
  EXPECT_EQ(store->GetObjectDisplayName(objects[2]->value),
            persistent_device_info->product_name.value());
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithVendorPolicyAndUserGrantedDevice) {
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      6353, 1000, "Vendor", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  const auto kVendorOrigin = url::Origin::Create(VendorUrl());
  store->GrantDevicePermission(kVendorOrigin, *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4u);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->value));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown product 0x162E from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*origin=*/CoolUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown product 0x054D from vendor 0x18D2");
}

TEST_F(UsbChooserContextTest,
       GetAllGrantedObjectsWithAnyPolicyAndUserGrantedDevice) {
  UsbDeviceInfoPtr persistent_device_info = device_manager_.CreateAndAddDevice(
      1123, 5813, "Some", "Product", "123ABC");

  auto* store = GetChooserContext(profile());
  profile()->GetPrefs()->Set(prefs::kManagedWebUsbAllowDevicesForUrls,
                             *base::JSONReader::ReadDeprecated(kPolicySetting));

  EXPECT_CALL(
      mock_permission_observer_,
      OnChooserObjectPermissionChanged(ContentSettingsType::USB_GUARD,
                                       ContentSettingsType::USB_CHOOSER_DATA));
  const auto kAnyDeviceOrigin = url::Origin::Create(AnyDeviceUrl());
  store->GrantDevicePermission(kAnyDeviceOrigin, *persistent_device_info);

  auto objects = store->GetAllGrantedObjects();
  ASSERT_EQ(objects.size(), 4u);

  for (const auto& object : objects) {
    EXPECT_TRUE(store->IsValidObject(object->value));
  }

  ExpectChooserObjectInfo(objects[0].get(),
                          /*origin=*/AnyDeviceUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/kDeviceIdWildcard,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from any vendor");
  ExpectChooserObjectInfo(objects[1].get(),
                          /*origin=*/VendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/kDeviceIdWildcard,
                          /*name=*/"Devices from Google Inc.");
  ExpectChooserObjectInfo(objects[2].get(),
                          /*origin=*/ProductVendorUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6353,
                          /*product_id=*/5678,
                          /*name=*/"Unknown product 0x162E from Google Inc.");
  ExpectChooserObjectInfo(objects[3].get(),
                          /*origin=*/CoolUrl(),
                          /*source=*/content_settings::SETTING_SOURCE_POLICY,
                          /*incognito=*/false,
                          /*vendor_id=*/6354,
                          /*product_id=*/1357,
                          /*name=*/"Unknown product 0x054D from vendor 0x18D2");
}

TEST_F(UsbChooserContextTest, MassStorageHidden) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);

  // Mass storage devices should be hidden.
  std::vector<device::mojom::UsbConfigurationInfoPtr> storage_configs;
  storage_configs.push_back(
      device::FakeUsbDeviceInfo::CreateConfiguration(0x08, 0x06, 0x50));
  UsbDeviceInfoPtr storage_device_info = device_manager_.CreateAndAddDevice(
      0, 0, "vendor1", "storage", "123ABC", std::move(storage_configs));

  // Composite devices with both mass storage and allowed interfaces should be
  // shown.
  std::vector<device::mojom::UsbConfigurationInfoPtr> complex_configs;
  complex_configs.push_back(
      device::FakeUsbDeviceInfo::CreateConfiguration(0x08, 0x06, 0x50, 1));
  complex_configs.push_back(
      device::FakeUsbDeviceInfo::CreateConfiguration(0xff, 0x42, 0x1, 2));
  UsbDeviceInfoPtr complex_device_info = device_manager_.CreateAndAddDevice(
      0, 0, "vendor2", "complex", "456DEF", std::move(complex_configs));

  UsbChooserContext* chooser_context = GetChooserContext(profile());

  base::RunLoop loop;
  chooser_context->GetDevices(
      base::BindLambdaForTesting([&](std::vector<UsbDeviceInfoPtr> devices) {
        EXPECT_EQ(1u, devices.size());
        EXPECT_EQ(complex_device_info->product_name, devices[0]->product_name);
        loop.Quit();
      }));
  loop.Run();
}

TEST_F(UsbChooserContextTest, DeviceWithNoInterfaceVisible) {
  GURL url("https://www.google.com");
  const auto origin = url::Origin::Create(url);

  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(
      device::FakeUsbDeviceInfo::CreateConfiguration(0x08, 0x06, 0x50));
  configs[0]->interfaces.clear();

  UsbDeviceInfoPtr device_info = device_manager_.CreateAndAddDevice(
      0, 0, "vendor1", "no_interface", "123ABC", std::move(configs));

  UsbChooserContext* chooser_context = GetChooserContext(profile());

  base::RunLoop loop;
  chooser_context->GetDevices(
      base::BindLambdaForTesting([&](std::vector<UsbDeviceInfoPtr> devices) {
        EXPECT_EQ(1u, devices.size());
        EXPECT_EQ(device_info->product_name, devices[0]->product_name);
        loop.Quit();
      }));
  loop.Run();
}