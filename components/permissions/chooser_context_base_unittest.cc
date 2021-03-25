// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_context_base.h"

#include "base/strings/strcat.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using ::testing::_;

namespace permissions {
namespace {

const char* kRequiredKey1 = "key-1";
const char* kRequiredKey2 = "key-2";

// TODO(https://crbug.com/1189682): For as long as we support object-based
// methods and extending GetKeyForObject is optional for base classes, this file
// will be a bit complicated.
// Currently:
//   - object-based methods are TYPED_TESTs (GetKeyForObject extended/not)
//   - key-based methods are TEST_Fs which only use TestKeyedChooserContext
// Once object-based methods are removed, typed tests will no longer be needed.

class TestNoKeyChooserContext : public ChooserContextBase {
 public:
  // This class uses the USB content settings type for testing purposes only.
  explicit TestNoKeyChooserContext(content::BrowserContext* browser_context)
      : ChooserContextBase(
            ContentSettingsType::USB_GUARD,
            ContentSettingsType::USB_CHOOSER_DATA,
            PermissionsClient::Get()->GetSettingsMap(browser_context)) {}
  ~TestNoKeyChooserContext() override = default;

  bool IsValidObject(const base::Value& object) override {
    return object.DictSize() == 2 && object.FindKey(kRequiredKey1) &&
           object.FindKey(kRequiredKey2);
  }

  std::u16string GetObjectDisplayName(const base::Value& object) override {
    return {};
  }
};

class TestKeyedChooserContext : public ChooserContextBase {
 public:
  // This class uses the USB content settings type for testing purposes only.
  explicit TestKeyedChooserContext(content::BrowserContext* browser_context)
      : ChooserContextBase(
            ContentSettingsType::USB_GUARD,
            ContentSettingsType::USB_CHOOSER_DATA,
            PermissionsClient::Get()->GetSettingsMap(browser_context)) {}
  ~TestKeyedChooserContext() override = default;

  bool IsValidObject(const base::Value& object) override {
    return object.DictSize() == 2 && object.FindKey(kRequiredKey1) &&
           object.FindKey(kRequiredKey2);
  }

  std::u16string GetObjectDisplayName(const base::Value& object) override {
    return {};
  }

  std::string GetKeyForObject(const base::Value& object) override {
    return *object.FindStringKey(kRequiredKey1);
  }
};

}  // namespace

class ChooserContextBaseTest : public testing::Test {
 public:
  ChooserContextBaseTest()
      : url1_("https://google.com"),
        url2_("https://chromium.org"),
        origin1_(url::Origin::Create(url1_)),
        origin2_(url::Origin::Create(url2_)),
        object1_(base::Value::Type::DICTIONARY),
        object2_(base::Value::Type::DICTIONARY) {
    object1_.SetStringKey(kRequiredKey1, "value1");
    object1_.SetStringKey(kRequiredKey2, "value2");
    object2_.SetStringKey(kRequiredKey1, "value3");
    object2_.SetStringKey(kRequiredKey2, "value4");
  }

  ~ChooserContextBaseTest() override = default;

  content::BrowserContext* browser_context() { return &browser_context_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
  TestPermissionsClient client_;

 protected:
  const GURL url1_;
  const GURL url2_;
  const url::Origin origin1_;
  const url::Origin origin2_;
  base::Value object1_;
  base::Value object2_;
};

template <typename T>
class ChooserContextBaseTypedTest : public ChooserContextBaseTest {
 public:
  ChooserContextBaseTypedTest()
      : ChooserContextBaseTest(), chooser_context_(browser_context()) {}

  ~ChooserContextBaseTypedTest() override = default;

 protected:
  T chooser_context_;
};

using TestChooserContext =
    testing::Types<TestNoKeyChooserContext, TestKeyedChooserContext>;

TYPED_TEST_SUITE(ChooserContextBaseTypedTest, TestChooserContext);

// ----------------------------OBJECT-BASED METHODS-----------------------------

TYPED_TEST(ChooserContextBaseTypedTest, GrantAndRevokeObjectPermissions) {
  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object2_.Clone());

  auto objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(this->object1_, objects[0]->value);
  EXPECT_EQ(this->object2_, objects[1]->value);

  // Granting permission to one origin should not grant them to another.
  objects = this->chooser_context_.GetGrantedObjects(this->origin2_);
  EXPECT_EQ(0u, objects.size());

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  EXPECT_CALL(mock_observer, OnPermissionRevoked(this->origin1_)).Times(2);
  this->chooser_context_.RevokeObjectPermission(this->origin1_, this->object1_);
  this->chooser_context_.RevokeObjectPermission(this->origin1_, this->object2_);
  objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(0u, objects.size());
}

TYPED_TEST(ChooserContextBaseTypedTest, GrantObjectPermissionTwice) {
  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());

  auto objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(this->object1_, objects[0]->value);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  EXPECT_CALL(mock_observer, OnPermissionRevoked(this->origin1_));
  this->chooser_context_.RevokeObjectPermission(this->origin1_, this->object1_);
  objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(0u, objects.size());
}

TYPED_TEST(ChooserContextBaseTypedTest, GrantAndUpdateObjectPermission) {
  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());

  auto objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(this->object1_, objects[0]->value);

  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  this->chooser_context_.UpdateObjectPermission(
      this->origin1_, objects[0]->value, this->object2_.Clone());

  objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(this->object2_, objects[0]->value);
}

// UpdateObjectPermission() should not grant new permissions.
TYPED_TEST(ChooserContextBaseTypedTest,
           UpdateObjectPermissionWithNonExistentPermission) {
  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  // Attempt to update permission for non-existent |this->object1_| permission.
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(0);
  this->chooser_context_.UpdateObjectPermission(this->origin1_, this->object1_,
                                                this->object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  auto objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_TRUE(objects.empty());

  // Grant permission for |this->object2_| but attempt to update permission for
  // non-existent |this->object1_| permission again.
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(0);
  this->chooser_context_.UpdateObjectPermission(this->origin1_, this->object1_,
                                                this->object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  objects = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(this->object2_, objects[0]->value);
}

TYPED_TEST(ChooserContextBaseTypedTest, GetAllGrantedObjects) {
  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());
  this->chooser_context_.GrantObjectPermission(this->origin2_,
                                               this->object2_.Clone());

  auto objects = this->chooser_context_.GetAllGrantedObjects();
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(this->object1_, objects[0]->value);
  EXPECT_EQ(this->object2_, objects[1]->value);
}

TYPED_TEST(ChooserContextBaseTypedTest, GetGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(this->browser_context());
  map->SetContentSettingDefaultScope(this->url1_, this->url1_,
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);

  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());
  this->chooser_context_.GrantObjectPermission(this->origin2_,
                                               this->object2_.Clone());

  auto objects1 = this->chooser_context_.GetGrantedObjects(this->origin1_);
  EXPECT_EQ(0u, objects1.size());

  auto objects2 = this->chooser_context_.GetGrantedObjects(this->origin2_);
  ASSERT_EQ(1u, objects2.size());
  EXPECT_EQ(this->object2_, objects2[0]->value);
}

TYPED_TEST(ChooserContextBaseTypedTest, GetAllGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(this->browser_context());
  map->SetContentSettingDefaultScope(this->url1_, this->url1_,
                                     ContentSettingsType::USB_GUARD,
                                     CONTENT_SETTING_BLOCK);

  MockPermissionObserver mock_observer;
  this->chooser_context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  this->chooser_context_.GrantObjectPermission(this->origin1_,
                                               this->object1_.Clone());
  this->chooser_context_.GrantObjectPermission(this->origin2_,
                                               this->object2_.Clone());

  auto objects = this->chooser_context_.GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(this->url2_, objects[0]->origin);
  EXPECT_EQ(this->object2_, objects[0]->value);
}

// ------------------------------KEY-BASED METHODS------------------------------

TEST_F(ChooserContextBaseTest, GrantAndRevokeObjectPermissions_Keyed) {
  TestKeyedChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  auto object1_key = context.GetKeyForObject(object1_);
  auto object2_key = context.GetKeyForObject(object2_);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, object1_.Clone());
  context.GrantObjectPermission(origin1_, object2_.Clone());

  auto objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);
  EXPECT_EQ(object2_, objects[1]->value);

  // Granting permission to one origin should not grant them to another.
  objects = context.GetGrantedObjects(origin2_);
  EXPECT_EQ(0u, objects.size());

  // Ensure objects can be retrieved individually.
  EXPECT_EQ(object1_, context.GetGrantedObject(origin1_, object1_key)->value);
  EXPECT_EQ(object2_, context.GetGrantedObject(origin1_, object2_key)->value);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_)).Times(2);
  context.RevokeObjectPermission(origin1_, object1_key);
  context.RevokeObjectPermission(origin1_, object2_key);
  EXPECT_EQ(nullptr, context.GetGrantedObject(origin1_, object1_key));
  EXPECT_EQ(nullptr, context.GetGrantedObject(origin1_, object2_key));
  objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantObjectPermissionTwice_Keyed) {
  TestKeyedChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, object1_.Clone());
  context.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_));
  context.RevokeObjectPermission(origin1_, context.GetKeyForObject(object1_));
  objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantAndUpdateObjectPermission_Keyed) {
  TestKeyedChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  context.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  // Update the object without changing the key.
  std::string new_value("new_value");
  auto new_object = objects[0]->value.Clone();
  new_object.SetStringKey(kRequiredKey2, new_value);
  EXPECT_NE(new_object, objects[0]->value);

  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  // GrantObjectPermission will update an object if an object with the same key
  // already exists.
  context.GrantObjectPermission(origin1_, new_object.Clone());

  objects = context.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(new_object, objects[0]->value);
}

}  // namespace permissions
