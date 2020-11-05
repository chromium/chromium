// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/chooser_context_base.h"

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/permissions/test/chooser_context_base_mock_permission_observer.h"
#include "components/permissions/test/test_permissions_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace permissions {
namespace {

const char* kRequiredKey1 = "key-1";
const char* kRequiredKey2 = "key-2";

class TestChooserContext : public ChooserContextBase {
 public:
  // This class uses the USB content settings type for testing purposes only.
  explicit TestChooserContext(content::BrowserContext* browser_context)
      : ChooserContextBase(
            ContentSettingsType::USB_GUARD,
            ContentSettingsType::USB_CHOOSER_DATA,
            PermissionsClient::Get()->GetSettingsMap(browser_context)) {}
  ~TestChooserContext() override {}

  bool IsValidObject(const base::Value& object) override {
    return object.DictSize() == 2 && object.FindKey(kRequiredKey1) &&
           object.FindKey(kRequiredKey2);
  }

  base::string16 GetObjectDisplayName(const base::Value& object) override {
    return {};
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

  ~ChooserContextBaseTest() override {}

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

TEST_F(ChooserContextBaseTest, GrantAndRevokeObjectPermissions) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());
  context.GrantObjectPermission(origin1_, origin1_, object2_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(2u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);
  EXPECT_EQ(object2_, objects[1]->value);

  // Granting permission to one origin should not grant them to another.
  objects = context.GetGrantedObjects(origin2_, origin2_);
  EXPECT_EQ(0u, objects.size());

  // Nor when the original origin is embedded in another.
  objects = context.GetGrantedObjects(origin1_, origin2_);
  EXPECT_EQ(0u, objects.size());

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_, origin1_)).Times(2);
  context.RevokeObjectPermission(origin1_, origin1_, object1_);
  context.RevokeObjectPermission(origin1_, origin1_, object2_);
  objects = context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantObjectPermissionTwice) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  EXPECT_CALL(mock_observer, OnPermissionRevoked(origin1_, origin1_));
  context.RevokeObjectPermission(origin1_, origin1_, object1_);
  objects = context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantObjectPermissionEmbedded) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  context.GrantObjectPermission(origin1_, origin2_, object1_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetGrantedObjects(origin1_, origin2_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  // The embedding origin still does not have permission.
  objects = context.GetGrantedObjects(origin2_, origin2_);
  EXPECT_EQ(0u, objects.size());

  // The requesting origin also doesn't have permission when not embedded.
  objects = context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects.size());
}

TEST_F(ChooserContextBaseTest, GrantAndUpdateObjectPermission) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object1_, objects[0]->value);

  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  context.UpdateObjectPermission(origin1_, origin1_, objects[0]->value,
                                 object2_.Clone());

  objects = context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(1u, objects.size());
  EXPECT_EQ(object2_, objects[0]->value);
}

// UpdateObjectPermission() should not grant new permissions.
TEST_F(ChooserContextBaseTest,
       UpdateObjectPermissionWithNonExistentPermission) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  // Attempt to update permission for non-existent |object1_| permission.
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(0);
  context.UpdateObjectPermission(origin1_, origin1_, object1_,
                                 object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_TRUE(objects.empty());

  // Grant permission for |object2_| but attempt to update permission for
  // non-existent |object1_| permission again.
  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _));
  context.GrantObjectPermission(origin1_, origin1_, object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(0);
  context.UpdateObjectPermission(origin1_, origin1_, object1_,
                                 object2_.Clone());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

TEST_F(ChooserContextBaseTest, GetAllGrantedObjects) {
  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());
  context.GrantObjectPermission(origin2_, origin2_, object2_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetAllGrantedObjects();
  EXPECT_EQ(2u, objects.size());
  bool found_one = false;
  bool found_two = false;
  for (const auto& object : objects) {
    if (object->requesting_origin == url1_) {
      EXPECT_FALSE(found_one);
      EXPECT_EQ(url1_, object->embedding_origin);
      EXPECT_EQ(object1_, objects[0]->value);
      found_one = true;
    } else if (object->requesting_origin == url2_) {
      EXPECT_FALSE(found_two);
      EXPECT_EQ(url2_, object->embedding_origin);
      EXPECT_EQ(object2_, objects[1]->value);
      found_two = true;
    } else {
      ADD_FAILURE() << "Unexpected object.";
    }
  }
  EXPECT_TRUE(found_one);
  EXPECT_TRUE(found_two);
}

TEST_F(ChooserContextBaseTest, GetGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  map->SetContentSettingDefaultScope(
      url1_, url1_, ContentSettingsType::USB_GUARD, CONTENT_SETTING_BLOCK);

  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());
  context.GrantObjectPermission(origin2_, origin2_, object2_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects1 =
      context.GetGrantedObjects(origin1_, origin1_);
  EXPECT_EQ(0u, objects1.size());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects2 =
      context.GetGrantedObjects(origin2_, origin2_);
  ASSERT_EQ(1u, objects2.size());
  EXPECT_EQ(object2_, objects2[0]->value);
}

TEST_F(ChooserContextBaseTest, GetAllGrantedObjectsWithGuardBlocked) {
  auto* map = PermissionsClient::Get()->GetSettingsMap(browser_context());
  map->SetContentSettingDefaultScope(
      url1_, url1_, ContentSettingsType::USB_GUARD, CONTENT_SETTING_BLOCK);

  TestChooserContext context(browser_context());
  MockPermissionObserver mock_observer;
  context.AddObserver(&mock_observer);

  EXPECT_CALL(mock_observer, OnChooserObjectPermissionChanged(_, _)).Times(2);
  context.GrantObjectPermission(origin1_, origin1_, object1_.Clone());
  context.GrantObjectPermission(origin2_, origin2_, object2_.Clone());

  std::vector<std::unique_ptr<ChooserContextBase::Object>> objects =
      context.GetAllGrantedObjects();
  ASSERT_EQ(1u, objects.size());
  EXPECT_EQ(url2_, objects[0]->requesting_origin);
  EXPECT_EQ(url2_, objects[0]->embedding_origin);
  EXPECT_EQ(object2_, objects[0]->value);
}

}  // namespace permissions
