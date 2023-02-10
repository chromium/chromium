// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/object_permission_context_base.h"

#include "base/strings/strcat.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/object_permission_context_base_mock_permission_observer.h"
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

class TestObjectPermissionContext : public ObjectPermissionContextBase {
 public:
  // This class uses the USB content settings type for testing purposes only.
  explicit TestObjectPermissionContext(content::BrowserContext* browser_context)
      : ObjectPermissionContextBase(
            ContentSettingsType::USB_GUARD,
            ContentSettingsType::USB_CHOOSER_DATA,
            PermissionsClient::Get()->GetSettingsMap(browser_context)) {}
  ~TestObjectPermissionContext() override = default;

  bool IsValidObject(const base::Value& object) override {
    const base::Value::Dict& dict = object.GetDict();
    return dict.size() == 2 && dict.Find(kRequiredKey1) &&
           dict.Find(kRequiredKey2);
  }

  std::u16string GetObjectDisplayName(const base::Value& object) override {
    return {};
  }

  std::string GetKeyForObject(const base::Value& object) override {
    return *object.GetDict().FindString(kRequiredKey1);
  }
};

}  // namespace

class ObjectPermissionContextBaseTest : public testing::Test {
 public:
  ObjectPermissionContextBaseTest()
      : url1_("https://google.com"),
        url2_("https://chromium.org"),
        origin1_(url::Origin::Create(url1_)),
        origin2_(url::Origin::Create(url2_)),
        object1_(base::Value::Type::DICT),
        object2_(base::Value::Type::DICT),
        context_(browser_context()) {
    object1_.GetDict().Set(kRequiredKey1, "value1");
    object1_.GetDict().Set(kRequiredKey2, "value2");
    object2_.GetDict().Set(kRequiredKey1, "value3");
    object2_.GetDict().Set(kRequiredKey2, "value4");
  }

  ~ObjectPermissionContextBaseTest() override = default;

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
  TestObjectPermissionContext context_;
};

TEST_F(ObjectPermissionContextBaseTest, GrantAndRevokeObjectPermissions) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin1_, object2_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);
  EXPECT_EQ(object2_, objects[1]->value);

  // Granting permission to one origin should not grant them to another.
  objects = context_.GetGrantedObjects(origin2_);
  EXPECT_EQ(0u, objects.size());

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_)).Times(2);
  context_.RevokeObjectPermission(origin1_, object1_);
  context_.RevokeObjectPermission(origin1_, object2_);
  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ObjectPermissionContextBaseTest, GrantObjectPermissionTwice) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_));
  context_.RevokeObjectPermission(origin1_, object1_);
  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ObjectPermissionContextBaseTest, GrantAndUpdateObjectPermission) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  context_.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  context_.UpdateObjectPermission(origin1_, objects[0]->value,
                                  object2_.Clone());

  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object2_, objects[0]->value);
}

// UpdateObjectPermission() should not grant new permissions.
TEST_F(ObjectPermissionContextBaseTest,
       UpdateObjectPermissionWithNonExistentPermission) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  // Attempt to update permission for non-existent |object1_| permission.
  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(0);
  context_.UpdateObjectPermission(origin1_, object1_, object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_TRUE(objects.empty());

  // Grant permission for |object2_| but attempt to update permission for
  // non-existent |object1_| permission again.
  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  context_.GrantObjectPermission(origin1_, object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(0);
  context_.UpdateObjectPermission(origin1_, object1_, object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object2_, objects[0]->value);
}

TEST_F(ObjectPermissionContextBaseTest, GetOriginsWithGrants) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin2_, object2_.Clone());

  auto origins_with_grants = context_.GetOriginsWithGrants();
  EXPECT_EQ(2u, origins_with_grants.size());
  EXPECT_EQ(origin2_, origins_with_grants[0]);
  EXPECT_EQ(origin1_, origins_with_grants[1]);
}

TEST_F(ObjectPermissionContextBaseTest, GetAllGrantedObjects) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin2_, object2_.Clone());

  auto objects = context_.GetAllGrantedObjects();
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(object2_, objects[0]->value);
  EXPECT_EQ(object1_, objects[1]->value);
}

TEST_F(ObjectPermissionContextBaseTest, GetGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  map->SetContentSettingDefaultScope(
      url1_, url1_, ContentSettingsType::USB_GUARD, CONTENT_SETTING_BLOCK);

  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin2_, object2_.Clone());

  auto objects1 = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects1.size());

  auto objects2 = context_.GetGrantedObjects(origin2_);
  ASSERT_EQ(1u, objects2.size());
  EXPECT_EQ(object2_, objects2[0]->value);
}

TEST_F(ObjectPermissionContextBaseTest, GetAllGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  map->SetContentSettingDefaultScope(
      url1_, url1_, ContentSettingsType::USB_GUARD, CONTENT_SETTING_BLOCK);

  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin2_, object2_.Clone());

  auto objects = context_.GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(url2_, objects[0]->origin);
  EXPECT_EQ(object2_, objects[0]->value);
}

TEST_F(ObjectPermissionContextBaseTest, GrantAndRevokeObjectPermissions_Keyed) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  auto object1_key = context_.GetKeyForObject(object1_);
  auto object2_key = context_.GetKeyForObject(object2_);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin1_, object2_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);
  EXPECT_EQ(object2_, objects[1]->value);

  // Granting permission to one origin should not grant them to another.
  objects = context_.GetGrantedObjects(origin2_);
  EXPECT_EQ(0u, objects.size());

  // Ensure objects can be retrieved individually.
  EXPECT_EQ(object1_, context_.GetGrantedObject(origin1_, object1_key)->value);
  EXPECT_EQ(object2_, context_.GetGrantedObject(origin1_, object2_key)->value);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_)).Times(2);
  context_.RevokeObjectPermission(origin1_, object1_key);
  context_.RevokeObjectPermission(origin1_, object2_key);
  EXPECT_EQ(nullptr, context_.GetGrantedObject(origin1_, object1_key));
  EXPECT_EQ(nullptr, context_.GetGrantedObject(origin1_, object2_key));
  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ObjectPermissionContextBaseTest, GrantObjectPermissionTwice_Keyed) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _)).Times(2);
  context_.GrantObjectPermission(origin1_, object1_.Clone());
  context_.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_));
  context_.RevokeObjectPermission(origin1_, context_.GetKeyForObject(object1_));
  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ObjectPermissionContextBaseTest, GrantAndUpdateObjectPermission_Keyed) {
  MockPermissionObserver mock_observer;
  context_.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  context_.GrantObjectPermission(origin1_, object1_.Clone());

  auto objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  // Update the object without changing the key.
  std::string new_value("new_value");
  auto new_object = objects[0]->value.Clone();
  new_object.SetStringKey(kRequiredKey2, new_value);
  EXPECT_NE(new_object, objects[0]->value);

  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnObjectPermissionChanged(_, _));
  // GrantObjectPermission will update an object if an object with the same key
  // already exists.
  context_.GrantObjectPermission(origin1_, new_object.Clone());

  objects = context_.GetGrantedObjects(origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(new_object, objects[0]->value);
}

}  // namespace permissions
