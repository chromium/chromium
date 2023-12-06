// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter.h"

#include <optional>

#include "base/gtest_prod_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Return;

class MockPageMetrics : public metrics_reporter::mojom::PageMetrics {
 public:
  MockPageMetrics() = default;
  ~MockPageMetrics() override = default;

  mojo::PendingRemote<metrics_reporter::mojom::PageMetrics> BindAndGetRemote() {
    DCHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }
  mojo::Receiver<metrics_reporter::mojom::PageMetrics> receiver_{this};

  MOCK_METHOD(void,
              OnGetMark,
              (const std::string&, OnGetMarkCallback),
              (override));
  MOCK_METHOD(void, OnClearMark, (const std::string&), (override));
};

class TestMetricsReporter : public MetricsReporter {
 public:
  using MetricsReporter::OnGetMark;
  using MetricsReporter::OnPageRemoteCreated;
};
class WebUIMetricsReporterTest : public BrowserWithTestWindowTest {
 public:
  WebUIMetricsReporterTest()
      : BrowserWithTestWindowTest(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {
    metrics_reporter_.OnPageRemoteCreated(page_metrics_.BindAndGetRemote());
  }

  MetricsReporter::OnGetMarkCallback TestGetMarkCallback(
      std::optional<base::TimeTicks> expected_time) {
    return base::BindOnce(
        [](std::optional<base::TimeTicks> expected_time,
           std::optional<base::TimeDelta> time) {
          EXPECT_EQ(expected_time.has_value(), time.has_value());
          if (time.has_value())
            EXPECT_EQ(time, expected_time->since_origin());
        },
        expected_time);
  }

  MetricsReporter::HasMarkCallback TestHasMarkCallback(bool expected_has_mark) {
    return base::BindOnce(
        [](bool expected_has_mark, bool has_mark) {
          EXPECT_EQ(expected_has_mark, expected_has_mark);
        },
        expected_has_mark);
  }

  MetricsReporter::MeasureCallback TestMeasureCallback(
      base::TimeDelta expected_duration) {
    return base::BindOnce(
        [](base::TimeDelta expected_duration, base::TimeDelta duration) {
          EXPECT_EQ(duration, base::Seconds(1));
        },
        expected_duration);
  }

 protected:
  const char* kHistogram = "TestHistogram";

  testing::StrictMock<MockPageMetrics> page_metrics_;
  TestMetricsReporter metrics_reporter_;
};

// Should be able to reply OnGetMarks().
TEST_F(WebUIMetricsReporterTest, OnGetMark) {
  EXPECT_CALL(page_metrics_, OnGetMark(_, _)).Times(0);
  EXPECT_CALL(page_metrics_, OnClearMark(_)).Times(0);

  const base::TimeTicks mark1 = base::TimeTicks::Now();
  metrics_reporter_.Mark("mark1");
  task_environment()->FastForwardBy(base::Seconds(1));
  const base::TimeTicks mark2 = base::TimeTicks::Now();
  metrics_reporter_.Mark("mark2");

  metrics_reporter_.OnGetMark("mark1", TestGetMarkCallback(mark1));
  metrics_reporter_.OnGetMark("mark2", TestGetMarkCallback(mark2));
}

// New marks should overrides old marks of same name.
TEST_F(WebUIMetricsReporterTest, OverridesMarks) {
  metrics_reporter_.Mark("mark-no-override");
  metrics_reporter_.Mark("mark-override");
  const base::TimeTicks old_mark = base::TimeTicks::Now();
  // Overrides an existing mark.
  task_environment()->FastForwardBy(base::Seconds(1));
  const base::TimeTicks new_mark = base::TimeTicks::Now();
  metrics_reporter_.Mark("mark-override");
  metrics_reporter_.OnGetMark("mark-override", TestGetMarkCallback(new_mark));
  metrics_reporter_.OnGetMark("mark-no-override",
                              TestGetMarkCallback(old_mark));
}

// HasMark() should check both local and remote marks.
TEST_F(WebUIMetricsReporterTest, HasMark) {
  EXPECT_CALL(page_metrics_, OnGetMark("nothing", _))
      .WillOnce([](const std::string& mark,
                   MetricsReporter::OnGetMarkCallback callback) {
        std::move(callback).Run(std::nullopt);
      });
  metrics_reporter_.HasMark("nothing", TestHasMarkCallback(false));

  metrics_reporter_.Mark("local_mark");
  metrics_reporter_.HasMark("local_mark", TestHasMarkCallback(true));

  EXPECT_CALL(page_metrics_, OnGetMark("remote_mark", _))
      .WillOnce([](const std::string& mark,
                   MetricsReporter::OnGetMarkCallback callback) {
        std::move(callback).Run(base::TimeTicks::Now().since_origin());
      });
  metrics_reporter_.HasMark("remote_mark", TestHasMarkCallback(true));
}

// HasLocalMark() should check only local marks.
TEST_F(WebUIMetricsReporterTest, HasLocalMark) {
  EXPECT_FALSE(metrics_reporter_.HasLocalMark("local_mark"));
  metrics_reporter_.Mark("local_mark");
  EXPECT_TRUE(metrics_reporter_.HasLocalMark("local_mark"));

  ON_CALL(page_metrics_, OnGetMark("remote_mark", _))
      .WillByDefault([](const std::string& mark,
                        MetricsReporter::OnGetMarkCallback callback) {
        std::move(callback).Run(base::TimeTicks::Now().since_origin());
      });
  EXPECT_FALSE(metrics_reporter_.HasLocalMark("remote_mark"));
}

// ClearMark() should clear both local and remote marks.
TEST_F(WebUIMetricsReporterTest, ClearMark) {
  EXPECT_CALL(page_metrics_, OnClearMark("remote_mark")).Times(1);
  EXPECT_CALL(page_metrics_, OnClearMark("local_mark")).Times(1);

  metrics_reporter_.Mark("local_mark");
  metrics_reporter_.ClearMark("local_mark");
  metrics_reporter_.ClearMark("remote_mark");
}

// Calling both Mark() and Measure() locally should not involve IPC.
TEST_F(WebUIMetricsReporterTest, MarkAndMeasureLocally) {
  EXPECT_CALL(page_metrics_, OnGetMark(_, _)).Times(0);
  EXPECT_CALL(page_metrics_, OnClearMark(_)).Times(0);

  metrics_reporter_.Mark("start_mark");
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics_reporter_.Measure("start_mark",
                            TestMeasureCallback(base::Seconds(1)));
}

// Measure() should accept an optional end mark.
TEST_F(WebUIMetricsReporterTest, MeasureWithEndMark) {
  EXPECT_CALL(page_metrics_, OnGetMark(_, _)).Times(0);
  EXPECT_CALL(page_metrics_, OnClearMark(_)).Times(0);

  metrics_reporter_.Mark("start_mark");
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics_reporter_.Mark("end_mark");
  metrics_reporter_.Measure("start_mark", "end_mark",
                            TestMeasureCallback(base::Seconds(1)));
}

// Measure() should be able to retrieve marks from remote.
TEST_F(WebUIMetricsReporterTest, MeasureRetrieveRemote) {
  const base::TimeTicks remote_mark = base::TimeTicks::Now();
  EXPECT_CALL(page_metrics_, OnGetMark("remote_mark", _))
      .WillOnce([remote_mark](const std::string& mark,
                              MetricsReporter::OnGetMarkCallback callback) {
        std::move(callback).Run(remote_mark.since_origin());
      });
  EXPECT_CALL(page_metrics_, OnClearMark(_)).Times(0);
  task_environment()->FastForwardBy(base::Seconds(1));
  metrics_reporter_.Measure("remote_mark",
                            TestMeasureCallback(base::Seconds(1)));
}
