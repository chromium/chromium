// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/single_sample_metrics_factory_impl.h"

#include "base/bind.h"
#include "base/metrics/dummy_histogram.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/metrics/single_sample_metrics.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

const base::HistogramBase::Sample kMin = 1;
const base::HistogramBase::Sample kMax = 10;
const uint32_t kBucketCount = 10;
const char kMetricName[] = "Single.Sample.Metric";

class SingleSampleMetricsFactoryImplTest : public testing::Test {
 public:
  SingleSampleMetricsFactoryImplTest() : thread_("TestThread") {
    InitializeSingleSampleMetricsFactory(
        base::BindRepeating(&SingleSampleMetricsFactoryImplTest::CreateProvider,
                            base::Unretained(this)));
    factory_ = static_cast<SingleSampleMetricsFactoryImpl*>(
        base::SingleSampleMetricsFactory::Get());
  }

  ~SingleSampleMetricsFactoryImplTest() override {
    factory_->DestroyProviderForTesting();
    if (thread_.IsRunning())
      ShutdownThread();
    base::SingleSampleMetricsFactory::DeleteFactoryForTesting();
  }

 protected:
  void StartThread() { ASSERT_TRUE(thread_.Start()); }

  void ShutdownThread() {
    thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SingleSampleMetricsFactoryImpl::DestroyProviderForTesting,
            base::Unretained(factory_)));
    thread_.Stop();
  }

  void CreateProvider(
      mojo::PendingReceiver<mojom::SingleSampleMetricsProvider> receiver) {
    CreateSingleSampleMetricsProvider(std::move(receiver));
    provider_count_++;
  }

  std::unique_ptr<base::SingleSampleMetric> CreateMetricOnThread() {
    std::unique_ptr<base::SingleSampleMetric> metric;
    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            &SingleSampleMetricsFactoryImplTest::CreateAndStoreMetric,
            base::Unretained(this), &metric),
        run_loop.QuitClosure());
    run_loop.Run();
    return metric;
  }

  void CreateAndStoreMetric(std::unique_ptr<base::SingleSampleMetric>* metric) {
    *metric = factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax,
                                                 kBucketCount);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  SingleSampleMetricsFactoryImpl* factory_;
  base::Thread thread_;
  size_t provider_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleSampleMetricsFactoryImplTest);
};

}  // namespace

TEST_F(SingleSampleMetricsFactoryImplTest, SingleProvider) {
  std::unique_ptr<base::SingleSampleMetric> metric1 =
      factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);

  std::unique_ptr<base::SingleSampleMetric> metric2 =
      factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);

  // Verify that only a single provider is created for multiple metrics.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, provider_count_);
}

TEST_F(SingleSampleMetricsFactoryImplTest, DoesNothing) {
  base::HistogramTester tester;

  std::unique_ptr<base::SingleSampleMetric> metric =
      factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);
  metric.reset();

  // Verify that no sample is recorded if SetSample() is never called.
  base::RunLoop().RunUntilIdle();
  tester.ExpectTotalCount(kMetricName, 0);
}

TEST_F(SingleSampleMetricsFactoryImplTest, DefaultSingleSampleMetricWithValue) {
  base::HistogramTester tester;
  std::unique_ptr<base::SingleSampleMetric> metric =
      factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);

  const base::HistogramBase::Sample kLastSample = 9;
  metric->SetSample(1);
  metric->SetSample(3);
  metric->SetSample(5);
  metric->SetSample(kLastSample);
  metric.reset();

  // Verify only the last sample sent to SetSample() is recorded.
  base::RunLoop().RunUntilIdle();
  tester.ExpectUniqueSample(kMetricName, kLastSample, 1);

  // Verify construction implicitly by requesting a histogram with the same
  // parameters; this test relies on the fact that histogram objects are unique
  // per name. Different parameters will result in a Dummy histogram returned.
  EXPECT_EQ(base::DummyHistogram::GetInstance(),
            base::Histogram::FactoryGet(kMetricName, 1, 3, 3,
                                        base::HistogramBase::kNoFlags));
  EXPECT_NE(base::DummyHistogram::GetInstance(),
            base::Histogram::FactoryGet(
                kMetricName, kMin, kMax, kBucketCount,
                base::HistogramBase::kUmaTargetedHistogramFlag));
}

// TODO(crbug.com/1009360). Flaky timeouts.
TEST_F(SingleSampleMetricsFactoryImplTest, DISABLED_MultithreadedMetrics) {
  base::HistogramTester tester;
  std::unique_ptr<base::SingleSampleMetric> metric =
      factory_->CreateCustomCountsMetric(kMetricName, kMin, kMax, kBucketCount);
  EXPECT_EQ(1u, provider_count_);

  StartThread();

  std::unique_ptr<base::SingleSampleMetric> threaded_metric =
      CreateMetricOnThread();
  ASSERT_TRUE(threaded_metric);

  // A second provider should be created to handle requests on our new thread.
  EXPECT_EQ(2u, provider_count_);

  // Calls from the wrong thread should DCHECK.
  EXPECT_DCHECK_DEATH(threaded_metric->SetSample(5));
  EXPECT_DCHECK_DEATH(threaded_metric.reset());

  // Test that samples are set on each thread correctly.
  const base::HistogramBase::Sample kSample = 7;

  {
    metric->SetSample(kSample);

    base::RunLoop run_loop;
    thread_.task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&base::SingleSampleMetric::SetSample,
                       base::Unretained(threaded_metric.get()), kSample),
        run_loop.QuitClosure());
    run_loop.Run();
  }

  // Release metrics and shutdown thread to ensure destruction completes.
  thread_.task_runner()->DeleteSoon(FROM_HERE, threaded_metric.release());
  ShutdownThread();

  metric.reset();
  base::RunLoop().RunUntilIdle();

  tester.ExpectUniqueSample(kMetricName, kSample, 2);
}

}  // namespace metrics
