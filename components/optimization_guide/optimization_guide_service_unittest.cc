// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/optimization_guide_service.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "components/optimization_guide/hints_component_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

const base::FilePath::CharType kFileName1[] = FILE_PATH_LITERAL("somefile1.pb");
const base::FilePath::CharType kFileName2[] = FILE_PATH_LITERAL("somefile2.pb");

class TestObserver : public OptimizationGuideServiceObserver {
 public:
  TestObserver()
      : hints_component_notification_count_(0),
        hints_component_version_("0.0.0.0") {}

  ~TestObserver() override {}

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
  int hints_component_notification_count_;
  base::Version hints_component_version_;
  base::FilePath hints_component_path_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class OptimizationGuideServiceTest : public testing::Test {
 public:
  OptimizationGuideServiceTest() {}

  ~OptimizationGuideServiceTest() override {}

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    optimization_guide_service_ = std::make_unique<OptimizationGuideService>(
        task_environment_.GetMainThreadTaskRunner());

    observer_ = std::make_unique<TestObserver>();
  }

  OptimizationGuideService* optimization_guide_service() {
    return optimization_guide_service_.get();
  }

  TestObserver* observer() { return observer_.get(); }

  void AddObserver() { optimization_guide_service_->AddObserver(observer()); }

  void RemoveObserver() {
    optimization_guide_service_->RemoveObserver(observer());
  }

  void MaybeUpdateHintsComponent(const HintsComponentInfo& info) {
    optimization_guide_service_->MaybeUpdateHintsComponent(info);
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  base::FilePath temp_dir() const { return temp_dir_.GetPath(); }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;

  std::unique_ptr<OptimizationGuideService> optimization_guide_service_;
  std::unique_ptr<TestObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideServiceTest);
};

TEST_F(OptimizationGuideServiceTest, ProcessHintsIssuesNotification) {
  base::HistogramTester histogram_tester;

  AddObserver();

  HintsComponentInfo component_info(base::Version("1.0.0.0"),
                                    temp_dir().Append(kFileName1));

  MaybeUpdateHintsComponent(component_info);

  EXPECT_EQ(observer()->hints_component_notification_count(), 1);
  EXPECT_EQ(component_info.version, observer()->hints_component_version());
  EXPECT_EQ(component_info.path, observer()->hints_component_path());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 1, 1);
}

TEST_F(OptimizationGuideServiceTest, ProcessHintsNewVersionProcessed) {
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
  // The histogram should be recorded twice - once for each update.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 1, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 2, 1);
}

TEST_F(OptimizationGuideServiceTest, ProcessHintsPastVersionIgnored) {
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
  // The histogram should only be recorded once - for the version it actually
  // updated to.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 2, 1);
}

TEST_F(OptimizationGuideServiceTest, ProcessHintsSameVersionIgnored) {
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
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 2, 1);
}

TEST_F(OptimizationGuideServiceTest,
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
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 1, 1);
}

TEST_F(OptimizationGuideServiceTest,
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
      "OptimizationGuide.OptimizationHintsComponent.MajorVersion", 172, 1);
}

}  // namespace optimization_guide
