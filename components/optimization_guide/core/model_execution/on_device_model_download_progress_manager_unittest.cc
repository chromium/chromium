// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_download_progress_manager.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "components/optimization_guide/core/model_execution/test/mock_download_progress_observer.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using component_updater::CrxUpdateItem;
using testing::_;
using update_client::ComponentState;

class OnDeviceModelDownloadProgressManagerTest : public testing::Test {
 public:
  OnDeviceModelDownloadProgressManagerTest() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kAIModelUnloadableProgress);
  }

  ~OnDeviceModelDownloadProgressManagerTest() override = default;

 protected:
  // Send a download update.
  void SendUpdate(FakeComponent& component,
                  ComponentState state,
                  uint64_t downloaded_bytes) {
    component_update_service_.SendUpdate(
        component.CreateUpdateItem(state, downloaded_bytes));
  }

  void SendUpdate(FakeComponent& component, uint64_t downloaded_bytes) {
    SendUpdate(component, ComponentState::kDownloading, downloaded_bytes);
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  FakeComponent& CreateComponent(std::string id, uint64_t total_bytes) {
    auto [iter, emplaced] = fake_components_.try_emplace(id, id, total_bytes);
    CHECK(emplaced);
    return iter->second;
  }

  FakeComponentUpdateService component_update_service_;

 private:
  void SetUp() override {
    EXPECT_CALL(component_update_service_, GetComponentDetails(_, _))
        .WillRepeatedly([&](const std::string& id, CrxUpdateItem* item) {
          auto iter = fake_components_.find(id);
          if (iter == fake_components_.end()) {
            return false;
          }

          if (iter->second.downloaded_bytes() == iter->second.total_bytes()) {
            *item = iter->second.CreateUpdateItem(
                update_client::ComponentState::kUpdated,
                iter->second.total_bytes());
          } else {
            *item = iter->second.CreateUpdateItem(
                update_client::ComponentState::kNew, 0);
          }

          return true;
        });
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::map<std::string, FakeComponent> fake_components_;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForNonDownloadEvents) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Doesn't receive any update for these event states.
  for (const auto state : {
           ComponentState::kNew,
           ComponentState::kChecking,
           ComponentState::kCanUpdate,
           ComponentState::kUpdated,
           ComponentState::kUpdateError,
           ComponentState::kRun,
       }) {
    SendUpdate(component, state, 10);
    observer.ExpectNoUpdate();
    FastForwardBy(base::Milliseconds(51));
  }
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForEventsWithNegativeDownloadedBytes) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Doesn't receive an update when the downloaded bytes are negative.
  SendUpdate(component, -1);
  observer.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForEventsWithNegativeTotalBytes) {
  FakeComponent& component = CreateComponent("component_id", -1);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Doesn't receive an update when the total bytes are negative.
  SendUpdate(component, 0);
  observer.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       DoesntReceiveUpdatesForComponentsNotObserving) {
  FakeComponent& component_observed = CreateComponent("component_id1", 100);
  FakeComponent& component_not_observed = CreateComponent("component_id2", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component_observed.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Doesn't receive any update for these event states.
  SendUpdate(component_not_observed, 10);
  observer.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ObservesComponentsMidDownload) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});
  MockDownloadProgressObserver observer1;
  MockDownloadProgressObserver observer2;

  // First, `observer1` observes `component`.
  manager.AddObserver(observer1.BindNewPipeAndPassRemote());

  // Only `observer1` will receive this update since `observer2` is not
  // observing.
  SendUpdate(component, 0);
  observer1.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  observer2.ExpectNoUpdate();

  // Update the download bytes, and only `observer1` will receive this update.
  uint64_t downloaded_bytes = 45;
  FastForwardBy(base::Milliseconds(51));
  SendUpdate(component, downloaded_bytes);
  observer1.ExpectReceivedNormalizedUpdate(downloaded_bytes,
                                           component.total_bytes());
  observer2.ExpectNoUpdate();

  // Now both `observer1` and `observer2` are observing `component`.
  // `observer2` haven't got any update, so no update notified.
  // For `observer2`, the total bytes will be the component's total bytes minus
  // downloaded bytes.
  manager.AddObserver(observer2.BindNewPipeAndPassRemote());

  // Send the first update for `observer2` waiting more than 50ms so that
  // both observers receive it.
  constexpr int64_t update1_for_observer2 = 60;
  FastForwardBy(base::Milliseconds(51));
  SendUpdate(component, update1_for_observer2);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure update_callback =
        base::BarrierClosure(2, run_loop.QuitClosure());

    // `observer1` should still be normalized against the total bytes of the
    // component.
    observer1.ExpectReceivedNormalizedUpdate(
        update1_for_observer2, component.total_bytes(), update_callback);
    // This is `observer2`'s first update so it should receive zero and be
    // normalized against the remaining bytes.
    observer2.ExpectReceivedNormalizedUpdate(
        0, component.total_bytes() - downloaded_bytes, update_callback);

    run_loop.Run();
  }

  // Send a second update for `observer2` waiting more than 50ms so that
  // both observers receive it.
  constexpr int64_t update2_for_observer2 = 75;
  FastForwardBy(base::Milliseconds(51));
  SendUpdate(component, update2_for_observer2);
  {
    base::RunLoop run_loop;
    base::RepeatingClosure update_callback =
        base::BarrierClosure(2, run_loop.QuitClosure());

    // `observer1` should still be normalized against the total bytes of the
    // component.
    observer1.ExpectReceivedNormalizedUpdate(
        update2_for_observer2, component.total_bytes(), update_callback);
    // `observer2` should still be normalized against the remaining bytes it
    // observed on its first update.
    observer2.ExpectReceivedNormalizedUpdate(
        update2_for_observer2 - downloaded_bytes,
        component.total_bytes() - downloaded_bytes, update_callback);

    run_loop.Run();
  }
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       DownloadedBytesWontExceedTotalBytes) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Send a zero, so that the `OnDeviceModelDownloadProgressManager` sends the
  // first update. This ensures that the already downloaded bytes is zero.
  SendUpdate(component, 0);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  FastForwardBy(base::Milliseconds(51));

  // Sending an update that exceeds the component's total bytes is clamped to
  // the component's total bytes.
  SendUpdate(component, component.total_bytes() * 2);
  observer.ExpectReceivedNormalizedUpdate(component.total_bytes(),
                                          component.total_bytes());
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ReporterIsDestroyedWhenRemoteIsDisconnected) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  // Should start with no reporters.
  EXPECT_EQ(manager.GetNumberOfReporters(), 0);

  {
    // Adding an Observer, should create a reporter.
    MockDownloadProgressObserver observer1;
    manager.AddObserver(observer1.BindNewPipeAndPassRemote());
    EXPECT_EQ(manager.GetNumberOfReporters(), 1);

    {
      // Adding an Observer, should create a reporter.
      MockDownloadProgressObserver observer2;
      manager.AddObserver(observer2.BindNewPipeAndPassRemote());
      EXPECT_EQ(manager.GetNumberOfReporters(), 2);
    }
    // `manager` should have destroyed the `Reporter` associated with
    // `observer2`.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return manager.GetNumberOfReporters() == 1; }));
  }
  // `manager` should have destroyed the `Reporter` associated with
  // `observer1`.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager.GetNumberOfReporters() == 0; }));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest, FirstUpdateIsReportedAsZero) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  // The first update should be reported as zero. And `total_bytes` should
  // always be `kNormalizedProgressMax` (0x10000).
  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  SendUpdate(component, 10);
  observer.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest, ProgressIsNormalized) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive the first update.
  SendUpdate(component, 0);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update should have its downloaded_bytes normalized.
  uint64_t downloaded_bytes = 15;
  uint64_t normalized_downloaded_bytes =
      NormalizeModelDownloadProgress(downloaded_bytes, component.total_bytes());

  SendUpdate(component, downloaded_bytes);
  observer.ExpectReceivedNormalizedUpdate(normalized_downloaded_bytes,
                                          kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       AlreadyDownloadedBytesArentIncludedInProgress) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  uint64_t already_downloaded_bytes = 10;

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Send the first update with the already downloaded bytes for `component`.
  SendUpdate(component, already_downloaded_bytes);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // The second update shouldn't include any already downloaded bytes.
  uint64_t downloaded_bytes = already_downloaded_bytes + 5;
  uint64_t normalized_downloaded_bytes = NormalizeModelDownloadProgress(
      downloaded_bytes - already_downloaded_bytes,
      component.total_bytes() - already_downloaded_bytes);
  SendUpdate(component, downloaded_bytes);
  observer.ExpectReceivedNormalizedUpdate(normalized_downloaded_bytes,
                                          kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsToTotalBytes) {
  FakeComponent& component =
      CreateComponent("component_id", kNormalizedDownloadProgressMax * 5);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive the zero update.
  SendUpdate(component, 10);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Sending less than the total bytes should not send the
  // `kNormalizedDownloadProgressMax`.
  SendUpdate(component, component.total_bytes() - 1);
  observer.ExpectReceivedUpdate(kNormalizedDownloadProgressMax - 1,
                                kNormalizedDownloadProgressMax);

  // Sending the total bytes should send the `kNormalizedDownloadProgressMax`.
  SendUpdate(component, component.total_bytes());
  observer.ExpectReceivedUpdate(kNormalizedDownloadProgressMax,
                                kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       MaxIsSentWhenDownloadedBytesEqualsToTotalBytesForFirstUpdate) {
  FakeComponent& component =
      CreateComponent("component_id", kNormalizedDownloadProgressMax * 5);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // If the first update has downloaded bytes equal to total bytes, then both
  // the the zero and max events should be fired.
  SendUpdate(component, component.total_bytes());
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());
  observer.ExpectReceivedUpdate(kNormalizedDownloadProgressMax,
                                kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ReceiveZeroAndHundredPercentForNoComponents) {
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               base::flat_set<std::string>{});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  observer.ExpectReceivedNormalizedUpdate(0, kNormalizedDownloadProgressMax);
  observer.ExpectReceivedNormalizedUpdate(kNormalizedDownloadProgressMax,
                                          kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest, OnlyReceivesUpdatesEvery50ms) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive the first update.
  SendUpdate(component, 0);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  SendUpdate(component, 15);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should receive the this since it's been over 50ms since the last update.
  SendUpdate(component, 20);
  observer.ExpectReceivedNormalizedUpdate(20, component.total_bytes());

  // Shouldn't receive this update since it hasn't been 50ms since the last
  // update.
  SendUpdate(component, 25);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       OnlyReceivesUpdatesForNewProgress) {
  // Set its total to twice kNormalizedProgressMax so that there are two raw
  // download progresses that map to every normalized download progress.
  FakeComponent& component =
      CreateComponent("component_id", kNormalizedDownloadProgressMax * 2);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive the first update as zero.
  SendUpdate(component, 0);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Should be able to receive this progress event since we haven't seen
  // it before.
  SendUpdate(component, 10);
  observer.ExpectReceivedNormalizedUpdate(10, component.total_bytes());

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive these progress updates since they're not
  // greater than the last progress update.
  SendUpdate(component, 10);
  SendUpdate(component, 9);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Shouldn't be able to receive this progress event since it normalizes
  // to a progress we've seen.
  CHECK_EQ(NormalizeModelDownloadProgress(10, component.total_bytes()),
           NormalizeModelDownloadProgress(11, component.total_bytes()));
  SendUpdate(component, 11);
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest, ShouldReceive100percent) {
  FakeComponent& component = CreateComponent("component_id", 100);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive the first update.
  SendUpdate(component, 10);
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

  // Should receive the second update since it's 100% even though 50ms haven't
  // elapsed.
  SendUpdate(component, component.total_bytes());
  observer.ExpectReceivedNormalizedUpdate(component.total_bytes(),
                                          component.total_bytes());

  // No other events should be fired.
  FastForwardBy(base::Milliseconds(51));
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       AllComponentsMustBeObservedBeforeSendingEvents) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_, {component1.id(), component2.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Shouldn't receive this updates since we haven't observed `component2` yet.
  SendUpdate(component1, 0);
  observer.ExpectNoUpdate();
  FastForwardBy(base::Milliseconds(51));

  // Should receive this update since now we've seen both components.
  SendUpdate(component2, 10);
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  observer.ExpectReceivedNormalizedUpdate(0, total_bytes);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ProgressIsNormalizedAgainstTheSumOfTheComponentsTotalBytes) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_, {component1.id(), component2.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Trigger the first event by sending updates for components 1 and 2.
  uint64_t component1_downloaded_bytes = 0;
  SendUpdate(component1, component1_downloaded_bytes);
  uint64_t component2_downloaded_bytes = 0;
  SendUpdate(component2, component2_downloaded_bytes);
  observer.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  SendUpdate(component2, component2_downloaded_bytes);

  // Should receive an update of the sum of component1 and component2's
  // downloaded bytes normalized with the sum of their total_bytes
  uint64_t downloaded_bytes =
      component1_downloaded_bytes + component2_downloaded_bytes;
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  uint64_t normalized_downloaded_bytes =
      NormalizeModelDownloadProgress(downloaded_bytes, total_bytes);

  observer.ExpectReceivedUpdate(normalized_downloaded_bytes,
                                kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       AlreadyDownloadedBytesArentIncludedInProgressForMultipleComponents) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_, {component1.id(), component2.id()});
  int64_t already_downloaded_bytes = 0;

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Send an update for component 1.
  uint64_t component1_downloaded_bytes = 5;
  already_downloaded_bytes += 5;
  SendUpdate(component1, component1_downloaded_bytes);

  // Send a second update for component 1. This increases the already downloaded
  // bytes that shouldn't be included in the progress.
  component1_downloaded_bytes += 5;
  already_downloaded_bytes += 5;
  SendUpdate(component1, component1_downloaded_bytes);

  // Send an update for component 2 triggering the zero event. This increases
  // the already downloaded bytes that shouldn't be included in the progress.
  uint64_t component2_downloaded_bytes = 10;
  already_downloaded_bytes += 10;
  SendUpdate(component2, component2_downloaded_bytes);
  observer.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Component 2 receives another 5 bytes.
  component2_downloaded_bytes += 5;
  SendUpdate(component2, component2_downloaded_bytes);

  // The progress we receive shouldn't include the `already_downloaded_bytes`.
  uint64_t downloaded_bytes =
      component1_downloaded_bytes + component2_downloaded_bytes;
  uint64_t total_bytes = component1.total_bytes() + component2.total_bytes();
  uint64_t normalized_downloaded_bytes = NormalizeModelDownloadProgress(
      downloaded_bytes - already_downloaded_bytes,
      total_bytes - already_downloaded_bytes);

  observer.ExpectReceivedUpdate(normalized_downloaded_bytes,
                                kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       AlreadyInstalledComponentsAreNotObserved) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_, {component1.id(), component2.id()});
  SendUpdate(component1, 100);

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Should receive this despite not observing component 1 yet since component1
  // is already downloaded.
  SendUpdate(component2, 0);
  observer.ExpectReceivedNormalizedUpdate(0, component2.total_bytes());
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ProgressIsNormalizedAgainstOnlyUninstalledComponents) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  FakeComponent& component3 = CreateComponent("component_id3", 500);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_,
      {component1.id(), component2.id(), component3.id()});
  SendUpdate(component1, 100);

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Fire the zero progress event by sending events for component 2 and 3.
  SendUpdate(component2, 0);
  SendUpdate(component3, 0);
  observer.ExpectReceivedUpdate(0, kNormalizedDownloadProgressMax);

  // Wait more than 50ms so we can receive the next event.
  FastForwardBy(base::Milliseconds(51));

  // Progress should be normalized against only components 2 and 3 since 1 is
  // already installed.
  SendUpdate(component2, 10);
  uint64_t total_bytes = component2.total_bytes() + component3.total_bytes();
  observer.ExpectReceivedNormalizedUpdate(10, total_bytes);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       ReceiveZeroAndHundredPercentWhenEverythingIsInstalled) {
  FakeComponent& component1 = CreateComponent("component_id1", 100);
  FakeComponent& component2 = CreateComponent("component_id2", 1000);
  OnDeviceModelDownloadProgressManager manager(
      &component_update_service_, {component1.id(), component2.id()});
  SendUpdate(component1, 100);
  SendUpdate(component2, 1000);

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  observer.ExpectReceivedNormalizedUpdate(0, kNormalizedDownloadProgressMax);
  observer.ExpectReceivedNormalizedUpdate(kNormalizedDownloadProgressMax,
                                          kNormalizedDownloadProgressMax);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest, UnloadableProgressBytes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAIModelUnloadableProgress,
      {{"ai_model_unloadable_progress_bytes", "500"}});

  FakeComponent& component = CreateComponent("component_id", 1000);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  SendUpdate(component, 0);
  // The total bytes should include the extra unloadable progress bytes.
  observer.ExpectReceivedNormalizedUpdate(0, component.total_bytes() + 500);
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       UnloadableProgressBytesAddedWhenComponentAlreadyInstalled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kAIModelUnloadableProgress,
      {{"ai_model_unloadable_progress_bytes", "500"}});

  FakeComponent& component = CreateComponent("component_id", 1000);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});
  SendUpdate(component, component.total_bytes());

  MockDownloadProgressObserver observer;
  manager.AddObserver(observer.BindNewPipeAndPassRemote());

  // Only receive the unloadable progress bytes since the component is already
  // installed.
  observer.ExpectReceivedNormalizedUpdate(0, 500);
  FastForwardBy(base::Milliseconds(51));
  observer.ExpectNoUpdate();
}

TEST_F(OnDeviceModelDownloadProgressManagerTest,
       AddNewObserverAfterRemoveObserver) {
  FakeComponent& component = CreateComponent("component_id", 1000);
  OnDeviceModelDownloadProgressManager manager(&component_update_service_,
                                               {component.id()});

  uint64_t downloaded_bytes = 100;
  {
    MockDownloadProgressObserver observer1;
    manager.AddObserver(observer1.BindNewPipeAndPassRemote());

    // The first update should reported as zero.
    SendUpdate(component, 0);
    observer1.ExpectReceivedNormalizedUpdate(0, component.total_bytes());

    // Wait more than 50ms so we can receive the next event.
    FastForwardBy(base::Milliseconds(51));

    // The second update should have its downloaded_bytes normalized.
    SendUpdate(component, downloaded_bytes);
    observer1.ExpectReceivedNormalizedUpdate(downloaded_bytes,
                                             component.total_bytes());
  }

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return manager.GetNumberOfReporters() == 0; }));

  // After remove all observers, we should recalculate the total bytes. Total
  // bytes should be reduced by the already downloaded bytes.
  {
    MockDownloadProgressObserver observer2;
    manager.AddObserver(observer2.BindNewPipeAndPassRemote());

    uint64_t new_downloaded_bytes = downloaded_bytes + 100;
    SendUpdate(component, new_downloaded_bytes);

    // The first update should reported as zero.
    observer2.ExpectReceivedNormalizedUpdate(
        0, component.total_bytes() - new_downloaded_bytes);

    // Wait more than 50ms so we can receive the next event.
    FastForwardBy(base::Milliseconds(51));

    // The second update should have its downloaded_bytes normalized.
    SendUpdate(component, new_downloaded_bytes + 100);
    observer2.ExpectReceivedNormalizedUpdate(
        100, component.total_bytes() - new_downloaded_bytes);
  }
}

}  // namespace optimization_guide
