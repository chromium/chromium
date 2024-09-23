// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_sharing/internal/android/data_sharing_service_android.h"

#include "base/android/jni_android.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/data_sharing/internal/data_sharing_service_impl.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/data_sharing/test_support/fake_data_sharing_sdk_delegate.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/data_sharing/internal/test_jni_headers/TestServiceObserver_jni.h"

namespace data_sharing {

using ::base::android::AttachCurrentThread;
using ::base::android::ScopedJavaLocalRef;

// Java observer for testing, counter part of TestServiceObserver.
// On each observation increments a counter and runs the callback.
class TestJavaObserver {
 public:
  TestJavaObserver(DataSharingService* service, base::OnceClosure callback)
      : service_(DataSharingService::GetJavaObject(service)),
        java_obj_(Java_TestServiceObserver_createAndAdd(
            AttachCurrentThread(),
            service_,
            reinterpret_cast<long>(this))),
        callback_(std::move(callback)) {}
  ~TestJavaObserver() {
    Java_TestServiceObserver_destroy(AttachCurrentThread(), java_obj_,
                                     service_);
  }

  int GetGroupChangeCount() {
    return Java_TestServiceObserver_getOnGroupChangeCount(AttachCurrentThread(),
                                                          java_obj_);
  }
  int GetGroupAddedCount() {
    return Java_TestServiceObserver_getOnGroupAddedCount(AttachCurrentThread(),
                                                         java_obj_);
  }
  int GetGroupRemovedCount() {
    return Java_TestServiceObserver_getOnGroupRemovedCount(
        AttachCurrentThread(), java_obj_);
  }

  void ResetCallback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

  void OnObserverNotify() { std::move(callback_).Run(); }

 private:
  ScopedJavaLocalRef<jobject> service_;
  ScopedJavaLocalRef<jobject> java_obj_;

  base::OnceClosure callback_;
};

// Implements TestServiceObserver.onObserverNotify static method.
void JNI_TestServiceObserver_OnObserverNotify(JNIEnv* env, jlong observer_ptr) {
  reinterpret_cast<TestJavaObserver*>(observer_ptr)->OnObserverNotify();
}

namespace {

sync_pb::CollaborationGroupSpecifics MakeCollaborationGroupSpecifics(
    const GroupId& id) {
  sync_pb::CollaborationGroupSpecifics result;
  result.set_collaboration_id(id.value());
  result.set_changed_at_timestamp_millis_since_unix_epoch(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  return result;
}

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  syncer::EntityData entity_data;
  *entity_data.specifics.mutable_collaboration_group() = specifics;
  entity_data.name = specifics.collaboration_id();
  return entity_data;
}

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateAdd(specifics.collaboration_id(),
                                         EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeUpdateFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateUpdate(specifics.collaboration_id(),
                                            EntityDataFromSpecifics(specifics));
}

std::unique_ptr<syncer::EntityChange> EntityChangeDeleteFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics) {
  return syncer::EntityChange::CreateDelete(specifics.collaboration_id());
}

}  // namespace

class DataSharingServiceAndroidTest : public testing::Test {
 public:
  DataSharingServiceAndroidTest() = default;

  ~DataSharingServiceAndroidTest() override = default;

  void SetUp() override {
    Test::SetUp();
    scoped_refptr<network::SharedURLLoaderFactory> test_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);

    std::unique_ptr<FakeDataSharingSDKDelegate> sdk_delegate =
        std::make_unique<FakeDataSharingSDKDelegate>();
    not_owned_sdk_delegate_ = sdk_delegate.get();

    data_sharing_service_ = std::make_unique<DataSharingServiceImpl>(
        std::move(test_url_loader_factory),
        identity_test_env_.identity_manager(),
        syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
        version_info::Channel::UNKNOWN, std::move(sdk_delegate),
        /*ui_delegate=*/nullptr);
    data_sharing_service_android_ = std::make_unique<DataSharingServiceAndroid>(
        data_sharing_service_.get());
  }

  void TearDown() override {
    data_sharing_service_android_.reset();
    not_owned_sdk_delegate_ = nullptr;
    data_sharing_service_.reset();
  }

  // Creates group and returns ID.
  // Mimics initial sync for collaboration group datatype, this should trigger
  // OnGroupAdded() notification.
  GroupId CreateGroup() {
    const std::string display_name = "display_name";
    const GroupId group_id =
        not_owned_sdk_delegate_->AddGroupAndReturnId(display_name);

    auto* collaboration_group_bridge =
        data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();

    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeAddFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->MergeFullSyncData(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
    return group_id;
  }

  // Removes the group with `group_id`, which would trigger the OnGroupRemoved()
  // notification.
  void RemoveGroup(const GroupId& group_id) {
    auto* collaboration_group_bridge =
        data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();
    not_owned_sdk_delegate_->RemoveGroup(group_id);
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeDeleteFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->ApplyIncrementalSyncChanges(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

  // Updates the group with `group_id` with a different name, which wuld trigger
  // the OnGroupUpdated() notification.
  void UpdateGroup(const GroupId& group_id) {
    const std::string new_display_name = "new_display_name";
    auto* collaboration_group_bridge =
        data_sharing_service_->GetCollaborationGroupSyncBridgeForTesting();
    not_owned_sdk_delegate_->UpdateGroup(group_id, new_display_name);
    syncer::EntityChangeList entity_changes;
    entity_changes.push_back(EntityChangeUpdateFromSpecifics(
        MakeCollaborationGroupSpecifics(group_id)));

    collaboration_group_bridge->ApplyIncrementalSyncChanges(
        collaboration_group_bridge->CreateMetadataChangeList(),
        std::move(entity_changes));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<DataSharingServiceImpl> data_sharing_service_;
  std::unique_ptr<DataSharingServiceAndroid> data_sharing_service_android_;
  raw_ptr<FakeDataSharingSDKDelegate> not_owned_sdk_delegate_;
};

TEST_F(DataSharingServiceAndroidTest, GroupAddedObservation) {
  base::RunLoop run_loop;
  TestJavaObserver observer(data_sharing_service_.get(),
                            run_loop.QuitClosure());

  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 0);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);

  CreateGroup();

  run_loop.Run();

  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 1);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);
}

TEST_F(DataSharingServiceAndroidTest, GroupRemovedObservation) {
  base::RunLoop run_loop;
  TestJavaObserver observer(data_sharing_service_.get(),
                            run_loop.QuitClosure());
  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 0);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);

  GroupId group_id = CreateGroup();

  run_loop.Run();
  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 1);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);

  base::RunLoop wait_for_remove;
  observer.ResetCallback(wait_for_remove.QuitClosure());

  RemoveGroup(group_id);

  wait_for_remove.Run();
  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 1);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 1);
}

TEST_F(DataSharingServiceAndroidTest, GroupChangeObservation) {
  base::RunLoop run_loop;
  TestJavaObserver observer(data_sharing_service_.get(),
                            run_loop.QuitClosure());
  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 0);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);

  GroupId group_id = CreateGroup();

  run_loop.Run();
  EXPECT_EQ(observer.GetGroupChangeCount(), 0);
  EXPECT_EQ(observer.GetGroupAddedCount(), 1);
  EXPECT_EQ(observer.GetGroupRemovedCount(), 0);

  base::RunLoop wait_for_update;
  observer.ResetCallback(wait_for_update.QuitClosure());

  UpdateGroup(group_id);

  wait_for_update.Run();
}

}  // namespace data_sharing
