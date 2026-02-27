// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/skills_service_impl.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/mock_callback.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/hints/mock_optimization_guide_decider.h"
#include "components/skills/features.h"
#include "components/skills/proto/skill.pb.h"
#include "components/skills/public/skill.h"
#include "components/sync/engine/data_type_activation_response.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_controller_delegate.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/protocol/skill_specifics.pb.h"
#include "components/sync/test/data_type_store_test_util.h"
#include "components/sync/test/mock_data_type_worker.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace skills {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AtLeast;
using ::testing::ElementsAre;
using ::testing::Exactly;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Pointee;

// This must match kSkillsDownloaderGstaticUrl in skills_downloader.cc
inline constexpr char kSkillsDownloaderGstaticUrl[] =
    "https://www.gstatic.com/chrome/skills/first_party_skills_binary";

class MockSkillsServiceImpl : public SkillsServiceImpl {
 public:
  using SkillsServiceImpl::SkillsServiceImpl;

  MOCK_METHOD(void,
              Handle1pSkillsMap,
              (std::unique_ptr<SkillsMap> skills_map),
              (override));
};

MATCHER_P4(HasSkill, name, icon, prompt, description, "") {
  return arg.name == name && arg.icon == icon && arg.prompt == prompt &&
         arg.description == description;
}

MATCHER_P5(HasSkillWithSource,
           source_skill_id,
           name,
           icon,
           prompt,
           description,
           "") {
  return arg.source_skill_id == source_skill_id && arg.name == name &&
         arg.icon == icon && arg.prompt == prompt &&
         arg.description == description;
}

MATCHER_P(HasCreationTime, creation_time, "") {
  return arg.creation_time == creation_time;
}

MATCHER_P(HasLastUpdateTime, last_update_time, "") {
  return arg.last_update_time == last_update_time;
}

class MockObserver : public SkillsService::Observer {
 public:
  MOCK_METHOD(void,
              OnSkillUpdated,
              (std::string_view skill_id,
               SkillsService::UpdateSource update_source,
               bool is_position_changed));
  MOCK_METHOD(void, OnStatusChanged, ());
};

class SkillsServiceImplTest : public testing::Test {
 public:
  SkillsServiceImplTest()
      : local_store_(syncer::DataTypeStoreTestUtil::CreateInMemoryStoreForTest(
            syncer::SKILL)) {}

  // Initializes the service and connects to sync. Data is loaded from the
  // in-memory storage.
  void InitService() {
    InitServiceWithoutSync();

    // Connect to sync and wait for the service to be ready.
    ConnectSync();

    ASSERT_TRUE(WaitForServiceStatus(SkillsService::ServiceStatus::kReady));
  }

  // Waits for the service to be in the given status. Returns true if the
  // service is in the given status, false if the timeout is reached.
  bool WaitForServiceStatus(SkillsService::ServiceStatus status) {
    return base::test::RunUntil(
        [this, status]() { return service_->GetServiceStatus() == status; });
  }

  // Initializes the service without connecting to sync.
  void InitServiceWithoutSync() {
    CHECK(!service_) << "Service already initialized";

    service_ = std::make_unique<SkillsServiceImpl>(
        &mock_optimization_guide_decider_, version_info::Channel::UNKNOWN,
        syncer::DataTypeStoreTestUtil::FactoryForForwardingStore(
            local_store_.get()),
        test_url_loader_factory_.GetSafeWeakWrapper());
    observation_.Observe(service_.get());
    ASSERT_EQ(service_->GetServiceStatus(),
              SkillsService::ServiceStatus::kNotInitialized);

    // Wait for the service to be initialized, any status change is enough here.
    ASSERT_TRUE(base::test::RunUntil([this]() {
      return service_->GetServiceStatus() !=
             SkillsService::ServiceStatus::kNotInitialized;
    }));
  }

  void ResetService() {
    observation_.Reset();
    service_.reset();
  }

  // Connects to sync and waits until the sync is activated and the initial
  // download is completed.
  void ConnectSync() {
    base::WeakPtr<syncer::DataTypeControllerDelegate> controller_delegate =
        service_->GetControllerDelegate();
    CHECK(controller_delegate);

    syncer::DataTypeActivationRequest request;
    request.error_handler = base::DoNothing();
    base::test::TestFuture<std::unique_ptr<syncer::DataTypeActivationResponse>>
        sync_starting_cb;
    controller_delegate->OnSyncStarting(request,
                                        sync_starting_cb.GetCallback());
    ASSERT_TRUE(sync_starting_cb.Wait());

    std::unique_ptr<syncer::DataTypeActivationResponse> response =
        sync_starting_cb.Take();
    ASSERT_TRUE(response);

    std::unique_ptr<syncer::MockDataTypeWorker> worker =
        syncer::MockDataTypeWorker::CreateWorkerAndConnectSync(
            std::move(response));

    // Simulate an empty update from the server for the initial sync. This is
    // no-op for the following updates.
    worker->UpdateFromServer(/*updates=*/{});
  }

  ~SkillsServiceImplTest() override = default;

  SkillsServiceImpl& service() { return *service_; }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<syncer::DataTypeStore> local_store_;
  std::unique_ptr<SkillsServiceImpl> service_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  testing::NiceMock<MockObserver> mock_observer_;
  base::ScopedObservation<SkillsService, SkillsService::Observer> observation_{
      &mock_observer_};
  testing::NiceMock<optimization_guide::MockOptimizationGuideDecider>
      mock_optimization_guide_decider_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SkillsServiceImplTest,
       DoesNotRegisterSkillsOptimizationTypeWhenFeatureDisabled) {
  scoped_feature_list_.InitAndDisableFeature(features::kSkillsEnabled);

  EXPECT_CALL(mock_optimization_guide_decider_, RegisterOptimizationTypes)
      .Times(0);

  InitService();
}

TEST_F(SkillsServiceImplTest,
       RegistersSkillsOptimizationTypeWhenFeatureEnabled) {
  scoped_feature_list_.InitAndEnableFeature(features::kSkillsEnabled);

  EXPECT_CALL(
      mock_optimization_guide_decider_,
      RegisterOptimizationTypes(ElementsAre(optimization_guide::proto::SKILLS)))
      .Times(Exactly(1));

  InitService();
}

TEST_F(SkillsServiceImplTest, LoadInitialSkills) {
  InitService();

  // Add some local skills which will be loaded after browser restart.
  service().AddSkill("source_skill_id", "name1", "icon1", "prompt1");
  service().AddSkill(/*source_skill_id=*/"", "name2", "icon2", "prompt2");
  ASSERT_THAT(
      service().GetSkills(),
      ElementsAre(
          Pointee(HasSkill("name2", "icon2", "prompt2", /*description=*/"")),
          Pointee(HasSkillWithSource("source_skill_id", "name1", "icon1",
                                     "prompt1", /*description=*/""))));

  // Simulate browser restart, it calls LoadInitialSkills() to load skills from
  // the disk implicitly.
  ResetService();
  InitService();

  EXPECT_THAT(
      service().GetSkills(),
      ElementsAre(
          Pointee(HasSkill("name2", "icon2", "prompt2", /*description=*/"")),
          Pointee(HasSkill("name1", "icon1", "prompt1", /*description=*/""))));
}

TEST_F(SkillsServiceImplTest, NotifyOnServiceStatusChange) {
  EXPECT_CALL(mock_observer_, OnStatusChanged).Times(AtLeast(1));
  InitServiceWithoutSync();
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_EQ(service().GetServiceStatus(),
            SkillsService::ServiceStatus::kInitializedWaitingForSyncReady);

  EXPECT_CALL(mock_observer_, OnStatusChanged);
  ConnectSync();
  EXPECT_TRUE(WaitForServiceStatus(SkillsService::ServiceStatus::kReady));
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // The service should be ready on browser restart even when sync is not
  // connected yet.
  ResetService();

  EXPECT_CALL(mock_observer_, OnStatusChanged).Times(testing::AtLeast(1));
  InitServiceWithoutSync();
  EXPECT_TRUE(WaitForServiceStatus(SkillsService::ServiceStatus::kReady));
}

TEST_F(SkillsServiceImplTest, GetSkillById) {
  InitService();

  service().AddOrUpdateSkillFromSync("id", /*source_skill_id=*/"", "name",
                                     "icon", "prompt", "description",
                                     /*creation_time=*/base::Time::Now(),
                                     /*last_update_time=*/base::Time::Now(),
                                     sync_pb::SKILL_SOURCE_USER_CREATED);

  const Skill* skill = service().GetSkillById("id");
  ASSERT_NE(nullptr, skill);
  EXPECT_EQ("name", skill->name);

  const Skill* null_skill = service().GetSkillById("non_existent_id");
  EXPECT_EQ(nullptr, null_skill);
}

TEST_F(SkillsServiceImplTest, AddSkill) {
  InitService();

  const Skill* added_skill = service().AddSkill(
      /*source_skill_id=*/"source_skill_id", "name", "icon", "prompt");

  ASSERT_NE(nullptr, added_skill);
  EXPECT_EQ("name", added_skill->name);
  EXPECT_EQ("icon", added_skill->icon);
  EXPECT_EQ("prompt", added_skill->prompt);
  EXPECT_EQ("source_skill_id", added_skill->source_skill_id);
  EXPECT_EQ(sync_pb::SkillSource::SKILL_SOURCE_DERIVED_FROM_FIRST_PARTY,
            added_skill->source);
  EXPECT_EQ(1u, service().GetSkills().size());
  EXPECT_FALSE(added_skill->id.empty());
  EXPECT_TRUE(base::Uuid::ParseLowercase(added_skill->id).is_valid());
  EXPECT_FALSE(added_skill->creation_time.is_null());
  EXPECT_EQ(added_skill->creation_time, added_skill->last_update_time);
}

TEST_F(SkillsServiceImplTest, UpdateSkill) {
  InitService();

  const Skill* skill = service().AddSkill(/*source_skill_id=*/"source_id",
                                          "name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  const base::Time initial_update_time = skill->last_update_time;

  const Skill* updated_skill =
      service().UpdateSkill(skill->id, "updated_name", "icon", "prompt");

  ASSERT_NE(nullptr, updated_skill);
  EXPECT_EQ("updated_name", updated_skill->name);
  EXPECT_EQ(1u, service().GetSkills().size());
  EXPECT_EQ(skill->id, updated_skill->id);
  EXPECT_GT(updated_skill->last_update_time, initial_update_time);
}

TEST_F(SkillsServiceImplTest, DeleteSkill) {
  InitService();

  const Skill* skill =
      service().AddSkill(/*source_skill_id=*/"", "name", "icon", "prompt");
  std::string skill_id(skill->id);
  ASSERT_NE(nullptr, skill);
  ASSERT_NE(nullptr, service().GetSkillById(skill_id));

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill_id, SkillsService::UpdateSource::kLocal,
                             /*is_position_changed=*/false));
  service().DeleteSkill(skill_id, SkillsService::UpdateSource::kLocal);
  EXPECT_EQ(nullptr, service().GetSkillById(skill_id));
}

TEST_F(SkillsServiceImplTest, DeleteSkillFromSync) {
  InitService();

  const Skill* skill =
      service().AddSkill(/*source_skill_id=*/"", "name", "icon", "prompt");
  ASSERT_NE(nullptr, skill);

  std::string skill_id(skill->id);
  ASSERT_NE(nullptr, service().GetSkillById(skill_id));

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill_id, SkillsService::UpdateSource::kSync,
                             /*is_position_changed=*/false));
  service().DeleteSkill(skill_id, SkillsService::UpdateSource::kSync);
  EXPECT_EQ(nullptr, service().GetSkillById(skill_id));
}

TEST_F(SkillsServiceImplTest, Observer) {
  EXPECT_CALL(mock_observer_, OnStatusChanged).Times(AtLeast(1));
  InitService();

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(_, SkillsService::UpdateSource::kLocal,
                             /*is_position_changed=*/true));
  const Skill* skill =
      service().AddSkill(/*source_skill_id=*/"", "name", "icon", "prompt");

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kLocal,
                             /*is_position_changed=*/false));
  service().UpdateSkill(skill->id, "updated_name", "icon", "prompt");

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kLocal,
                             /*is_position_changed=*/false));
  service().DeleteSkill(skill->id, SkillsService::UpdateSource::kLocal);
}

TEST_F(SkillsServiceImplTest, ObserverNoNotificationForNoOps) {
  EXPECT_CALL(mock_observer_, OnStatusChanged).Times(AtLeast(1));
  InitService();
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // `UpdateSkill` and `DeleteSkill` on a non-existent skill should not
  // trigger notification.
  const std::string non_existent_skill_id = "non_existent_skill_id";
  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(0);
  service().UpdateSkill(non_existent_skill_id, "name", "icon", "prompt");
  service().DeleteSkill(non_existent_skill_id,
                        SkillsService::UpdateSource::kLocal);
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  // `UpdateSkill` with the same values should not trigger notification.
  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(1);
  const Skill* skill =
      service().AddSkill(/*source_skill_id=*/"", "name", "icon", "prompt");
  std::string skill_id = skill->id;
  testing::Mock::VerifyAndClearExpectations(&mock_observer_);

  EXPECT_CALL(mock_observer_, OnSkillUpdated).Times(0);
  service().UpdateSkill(skill_id, "name", "icon", "prompt");
}

TEST_F(SkillsServiceImplTest, UpdateExistingSkillFromSync) {
  InitService();

  // Add an initial skill.
  const Skill* skill =
      service().AddSkill(/*source_skill_id=*/"", "name", "icon", "prompt");
  const base::Time initial_creation_time = skill->creation_time;

  const base::Time new_update_time = skill->last_update_time + base::Hours(1);
  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(skill->id, SkillsService::UpdateSource::kSync,
                             /*is_position_changed=*/false));
  const Skill* updated_skill = service().AddOrUpdateSkillFromSync(
      skill->id, /*source_skill_id=*/"", "sync name", "sync icon",
      "sync prompt", "sync description", initial_creation_time,
      /*last_update_time=*/new_update_time, sync_pb::SKILL_SOURCE_USER_CREATED);

  // Only the last update time should be updated.
  ASSERT_EQ(skill, updated_skill);
  EXPECT_THAT(
      service().GetSkills(),
      ElementsAre(Pointee(AllOf(
          HasSkill("sync name", "sync icon", "sync prompt", "sync description"),
          HasCreationTime(initial_creation_time),
          HasLastUpdateTime(new_update_time)))));
}

TEST_F(SkillsServiceImplTest, AddSkillFromSync) {
  const base::Time creation_time = base::Time::Now() - base::Days(1);
  const base::Time update_time = creation_time + base::Hours(1);

  InitService();

  EXPECT_CALL(mock_observer_,
              OnSkillUpdated(_, SkillsService::UpdateSource::kSync,
                             /*is_position_changed=*/true));

  const Skill* skill = service().AddOrUpdateSkillFromSync(
      "id", "source_skill_id", "name", "icon", "prompt", "description",
      creation_time, update_time, sync_pb::SKILL_SOURCE_FIRST_PARTY);
  ASSERT_NE(nullptr, skill);

  EXPECT_THAT(
      service().GetSkills(),
      ElementsAre(Pointee(AllOf(
          HasSkillWithSource("source_skill_id", "name", "icon", "prompt",
                             "description"),
          HasCreationTime(creation_time), HasLastUpdateTime(update_time)))));
}

TEST_F(SkillsServiceImplTest, FetchDiscoverySkills_Success) {
  scoped_feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  skills::proto::SkillsList skills_list;
  skills_list.add_skills()->set_name("/test-skill-only-name");
  test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl,
                                       skills_list.SerializeAsString());
  MockSkillsServiceImpl mock_service(
      &mock_optimization_guide_decider_, version_info::Channel::UNKNOWN,
      syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
      test_url_loader_factory_.GetSafeWeakWrapper());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_service, Handle1pSkillsMap(_))
      .WillOnce([&](std::unique_ptr<SkillsService::SkillsMap> skills_map) {
        EXPECT_EQ(1u, skills_map->size());
        run_loop.Quit();
      });

  mock_service.FetchDiscoverySkills();
  run_loop.Run();
}

TEST_F(SkillsServiceImplTest, FetchDiscoverySkills_Failure) {
  scoped_feature_list_.InitAndEnableFeature(features::kSkillsEnabled);
  test_url_loader_factory_.AddResponse(kSkillsDownloaderGstaticUrl, "",
                                       net::HTTP_NOT_FOUND);
  MockSkillsServiceImpl mock_service(
      &mock_optimization_guide_decider_, version_info::Channel::UNKNOWN,
      syncer::DataTypeStoreTestUtil::FactoryForInMemoryStoreForTest(),
      test_url_loader_factory_.GetSafeWeakWrapper());

  base::RunLoop run_loop;
  EXPECT_CALL(mock_service, Handle1pSkillsMap(testing::IsNull()))
      .WillOnce([&](std::unique_ptr<SkillsService::SkillsMap> skills_map) {
        EXPECT_FALSE(skills_map);
        run_loop.Quit();
      });

  mock_service.FetchDiscoverySkills();
  run_loop.Run();
}

TEST_F(SkillsServiceImplTest, AddSkillSortsByLastUpdateTime) {
  InitService();
  service().AddSkill("source_id", "Name B", "icon", "prompt");
  service().AddSkill("source_id", "Name A", "icon", "prompt");
  service().AddSkill("source_id", "Name C", "icon", "prompt");

  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill("Name C", "icon", "prompt", "")),
                          Pointee(HasSkill("Name A", "icon", "prompt", "")),
                          Pointee(HasSkill("Name B", "icon", "prompt", ""))));
}

TEST_F(SkillsServiceImplTest, UpdateSkillSortsByLastUpdateTime) {
  InitService();
  const Skill* skill1 =
      service().AddSkill("source_id", "Name A", "icon", "prompt");
  service().AddSkill("source_id", "Name B", "icon", "prompt");
  service().AddSkill("source_id", "Name C", "icon", "prompt");

  // Update "A" to "D". New order should be D, C, B.
  service().UpdateSkill(skill1->id, "Name D", "icon", "prompt");

  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill("Name D", "icon", "prompt", "")),
                          Pointee(HasSkill("Name C", "icon", "prompt", "")),
                          Pointee(HasSkill("Name B", "icon", "prompt", ""))));
}

TEST_F(SkillsServiceImplTest, AddSkillFromSyncSortsByLastUpdateTime) {
  InitService();
  service().AddSkill("source_id", "Name B", "icon", "prompt");

  service().AddOrUpdateSkillFromSync(
      "id_A", "source_id", "Name A", "icon", "prompt", "desc",
      base::Time::Now(), base::Time::Now(), sync_pb::SKILL_SOURCE_USER_CREATED);

  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill("Name A", "icon", "prompt", "desc")),
                          Pointee(HasSkill("Name B", "icon", "prompt", ""))));
}

TEST_F(SkillsServiceImplTest, UpdateSkillFromSyncSortsByLastUpdateTime) {
  InitService();
  const Skill* skill1 =
      service().AddSkill("source_id", "Name A", "icon", "prompt");
  service().AddSkill("source_id", "Name B", "icon", "prompt");

  // Update "A" to "C" via Sync. Order should become C, B.
  service().AddOrUpdateSkillFromSync(skill1->id, "source_id", "Name C", "icon",
                                     "prompt", "desc", base::Time::Now(),
                                     base::Time::Now() + base::Seconds(1),
                                     sync_pb::SKILL_SOURCE_USER_CREATED);

  EXPECT_THAT(service().GetSkills(),
              ElementsAre(Pointee(HasSkill("Name C", "icon", "prompt", "desc")),
                          Pointee(HasSkill("Name B", "icon", "prompt", ""))));
}

}  // namespace
}  // namespace skills
