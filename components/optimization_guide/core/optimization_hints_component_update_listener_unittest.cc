// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/optimization_guide/core/hints_component_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

const base::FilePath::CharType kFileName1[] = FILE_PATH_LITERAL("somefile1.pb");
const base::FilePath::CharType kFileName2[] = FILE_PATH_LITERAL("somefile2.pb");

class TestObserver : public OptimizationHintsComponentObserver {
 public:
  TestObserver() : hints_component_version_("0.0.0.0") {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  ~TestObserver() override = default;

  void OnHintsComponentAvailable(const HintsComponentInfo& info) override {
    ++hints_component_notification_count_;
    hints_component_version_ = info.version;
    hints_component_path_ = info.path;
  }

  int hints_component_notification_count() const {
    return hints_component_notification_count_;
  }
  const base::Version& hints_component_version() const {
    return hints_component_version_;
  }
  const base::FilePath& hints_component_path() const {
    return hints_component_path_;
  }

 private:
  int hints_component_notification_count_ = 0;
  base::Version hints_component_version_;
  base::FilePath hints_component_path_;
};

class OptimizationHintsComponentUpdateListenerTest : public testing::Test {
 public:
  OptimizationHintsComponentUpdateListenerTest() = default;

  OptimizationHintsComponentUpdateListenerTest(
      const OptimizationHintsComponentUpdateListenerTest&) = delete;
  OptimizationHintsComponentUpdateListenerTest& operator=(
      const OptimizationHintsComponentUpdateListenerTest&) = delete;

  ~OptimizationHintsComponentUpdateListenerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    OptimizationHintsComponentUpdateListener::GetInstance()
        ->ResetStateForTesting();

    observer_ = std::make_unique<TestObserver>();
  }

  void TearDown() override { RemoveObserver(); }

  TestObserver* observer() { return observer_.get(); }

  void AddObserver() {
    OptimizationHintsComponentUpdateListener::GetInstance()->AddObserver(
        observer());
  }

  void RemoveObserver() {
    OptimizationHintsComponentUpdateListener::GetInstance()->RemoveObserver(
        observer());
  }

  void MaybeUpdateHintsComponent(const HintsComponentInfo& info) {
    OptimizationHintsComponentUpdateListener::GetInstance()
        ->MaybeUpdateHintsComponent(info);
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<TestObserver> observer_;
};

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       ProcessHintsIssuesNotification) {
  base::HistogramTester histogram_tester;

  AddObserver();

  HintsComponentInfo component_info(base::Version("1.0.0.0"),
                                    temp_dir().Append(kFileName1));

  MaybeUpdateHintsComponent(component_info);

  EXPECT_EQ(observer()->hints_component_notification_count(), 1);
  EXPECT_EQ(component_info.version, observer()->hints_component_version());
  EXPECT_EQ(component_info.path, observer()->hints_component_path());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 1, 1);
}

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       ProcessHintsNewVersionProcessed) {
  base::HistogramTester histogram_tester;

  AddObserver();

  HintsComponentInfo component_info_1(base::Version("1.0.0.0"),
                                      temp_dir().Append(kFileName1));
  HintsComponentInfo component_info_2(base::Version("2.0.0.0"),
                                      temp_dir().Append(kFileName2));

  MaybeUpdateHintsComponent(component_info_1);
  MaybeUpdateHintsComponent(component_info_2);

  EXPECT_EQ(observer()->hints_component_notification_count(), 2);
  EXPECT_EQ(component_info_2.version, observer()->hints_component_version());
  EXPECT_EQ(component_info_2.path, observer()->hints_component_path());
  // The histogram should be recorded each time the component is loaded.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 2, 1);
}

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       ProcessHintsPastVersionIgnored) {
  base::HistogramTester histogram_tester;

  AddObserver();

  HintsComponentInfo component_info_1(base::Version("2.0.0.0"),
                                      temp_dir().Append(kFileName1));
  HintsComponentInfo component_info_2(base::Version("1.0.0.0"),
                                      temp_dir().Append(kFileName2));

  MaybeUpdateHintsComponent(component_info_1);
  MaybeUpdateHintsComponent(component_info_2);

  EXPECT_EQ(observer()->hints_component_notification_count(), 1);
  EXPECT_EQ(component_info_1.version, observer()->hints_component_version());
  EXPECT_EQ(component_info_1.path, observer()->hints_component_path());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 2, 1);
}

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       ProcessHintsSameVersionIgnored) {
  base::HistogramTester histogram_tester;

  AddObserver();

  HintsComponentInfo component_info_1(base::Version("2.0.0.0"),
                                      temp_dir().Append(kFileName1));
  HintsComponentInfo component_info_2(base::Version("2.0.0.0"),
                                      temp_dir().Append(kFileName2));

  MaybeUpdateHintsComponent(component_info_1);
  MaybeUpdateHintsComponent(component_info_2);

  EXPECT_EQ(observer()->hints_component_notification_count(), 1);
  EXPECT_EQ(component_info_1.version, observer()->hints_component_version());
  EXPECT_EQ(component_info_1.path, observer()->hints_component_path());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 2, 2);
}

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       UnregisteredObserverDoesNotReceiveNotification) {
  base::HistogramTester histogram_tester;

  // Add and remove observer to ensure that observer properly unregistered.
  AddObserver();
  RemoveObserver();

  HintsComponentInfo component_info(base::Version("1.0.0.0"),
                                    temp_dir().Append(kFileName1));

  MaybeUpdateHintsComponent(component_info);

  EXPECT_EQ(observer()->hints_component_notification_count(), 0);
  // We should still log the histogram since that is what the component updater
  // storage has.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 1, 1);
}

TEST_F(OptimizationHintsComponentUpdateListenerTest,
       RegisteredObserverReceivesNotificationForCurrentComponent) {
  base::HistogramTester histogram_tester;

  HintsComponentInfo component_info(base::Version("172"),
                                    temp_dir().Append(kFileName1));

  MaybeUpdateHintsComponent(component_info);

  AddObserver();

  EXPECT_EQ(observer()->hints_component_notification_count(), 1);
  EXPECT_EQ(component_info.version, observer()->hints_component_version());
  EXPECT_EQ(component_info.path, observer()->hints_component_path());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion2", 172, 1);
}

}  // namespace optimization_guide
