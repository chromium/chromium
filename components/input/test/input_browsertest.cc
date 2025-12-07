// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
#include "components/input/features.h"
#include "components/input/utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"

namespace input {

class InputBrowserTest : public content::ContentBrowserTest {
 public:
  bool IsRenderInputRouterCreatedOnViz() {
    base::test::TestTraceProcessor ttp;
    ttp.StartTrace("input");
    content::RenderFrameSubmissionObserver render_frame_submission_observer(
        shell()->web_contents());

    EXPECT_TRUE(content::NavigateToURL(
        shell(), GURL("data:text/html,<!doctype html>"
                      "<body style='background-color: magenta;'></body>")));
    if (render_frame_submission_observer.render_frame_count() == 0) {
      render_frame_submission_observer.WaitForAnyFrameSubmission();
    }

    absl::Status status = ttp.StopAndParseTrace();
    EXPECT_TRUE(status.ok()) << status.message();

    std::string query = R"(SELECT COUNT(*) as cnt
                       FROM slice
                       JOIN thread_track ON slice.track_id = thread_track.id
                       JOIN thread USING(utid)
                       WHERE slice.name = 'RenderInputRouter::RenderInputRouter'
                       AND thread.name = 'VizCompositorThread'
                       )";
    auto result = ttp.RunQuery(query);

    EXPECT_TRUE(result.has_value());

    // `result.value()` would look something like this: {{"cnt"}, {"<num>"}.
    EXPECT_EQ(result.value().size(), 2u);
    EXPECT_EQ(result.value()[1].size(), 1u);
    const std::string slice_count = result.value()[1][0];
    return slice_count == "1";
  }
};

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(InputBrowserTest,
                       RenderInputRouterNotCreatedOnNonAndroid) {
  EXPECT_FALSE(IsRenderInputRouterCreatedOnViz());
}
#else
class AndroidInputBrowserTest : public InputBrowserTest,
                                public ::testing::WithParamInterface<bool> {
 public:
  AndroidInputBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(features::kInputOnViz,
                                              /* enabled= */ GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AndroidInputBrowserTest, RenderInputRouterCreation) {
  const bool expected_creation = InputUtils::IsTransferInputToVizSupported();
  EXPECT_EQ(IsRenderInputRouterCreatedOnViz(), expected_creation);
}

// Tests whether RenderWidgetHostInputEventRouter was created with different
// grouping id's for different WebContents.
IN_PROC_BROWSER_TEST_P(AndroidInputBrowserTest,
                       RenderWidgetHostInputEventRouterCreated) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("viz");

  GURL url = GURL(
      "data:text/html,<!doctype html>"
      "<body style='background-color: magenta;'></body>");

  // Loads a URL in two separate web contents.
  {
    content::RenderFrameSubmissionObserver render_frame_submission_observer(
        shell()->web_contents());
    ASSERT_TRUE(NavigateToURL(shell(), url));
    if (render_frame_submission_observer.render_frame_count() == 0) {
      render_frame_submission_observer.WaitForAnyFrameSubmission();
    }
  }

  {
    auto* second_shell = content::Shell::CreateNewWindow(
        shell()->web_contents()->GetBrowserContext(), url, nullptr, {800, 600});
    content::RenderFrameSubmissionObserver render_frame_submission_observer(
        second_shell->web_contents());
    if (render_frame_submission_observer.render_frame_count() == 0) {
      render_frame_submission_observer.WaitForAnyFrameSubmission();
    }
  }

  absl::Status status = ttp.StopAndParseTrace();
  EXPECT_TRUE(status.ok()) << status.message();

  // Select the count of distinct grouping_id's.
  std::string query = R"(
      SELECT COUNT(DISTINCT EXTRACT_ARG(arg_set_id, 'debug.grouping_id'))
      AS distinct_grouping_ids
      FROM slice
      WHERE slice.name = 'RenderWidgetHostInputEventRouterCreated'
    )";

  auto result = ttp.RunQuery(query);

  EXPECT_TRUE(result.has_value());

  // `result.value()` would look something like this: {{"id"}, {"<num>"},
  // {"<num>"}}.
  EXPECT_EQ(result.value()[0].size(), 1u);
  if (InputUtils::IsTransferInputToVizSupported()) {
    EXPECT_EQ(result.value().size(), 2u);
    // Expect the distinct grouping_id count to be 2 for different WebContents.
    EXPECT_THAT(
        result.value(),
        testing::ElementsAre(testing::ElementsAre("distinct_grouping_ids"),
                             testing::ElementsAre("2")));
  } else {
    EXPECT_EQ(result.value().size(), 2u);
    EXPECT_THAT(
        result.value(),
        testing::ElementsAre(testing::ElementsAre("distinct_grouping_ids"),
                             testing::ElementsAre("0")));
  }
}

IN_PROC_BROWSER_TEST_P(AndroidInputBrowserTest, AndroidInputReceiverCreated) {
  base::HistogramTester histogram_tester;
  content::RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  EXPECT_TRUE(content::NavigateToURL(
      shell(), GURL("data:text/html,<!doctype html>"
                    "<body style='background-color: magenta;'></body>")));
  if (render_frame_submission_observer.render_frame_count() == 0) {
    render_frame_submission_observer.WaitForAnyFrameSubmission();
  }
  // By this point a root compositor frame sink should have been created as
  // well, and if an input receiver were to be created it should have been
  // since it happens when root compositor frame sink is created.

  const int expected_count =
      InputUtils::IsTransferInputToVizSupported() ? 1 : 0;
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectUniqueSample(
      "Android.InputOnViz.InputReceiverCreationResult",
      /*kSuccessfullyCreated*/ 0, expected_count);
}

IN_PROC_BROWSER_TEST_P(AndroidInputBrowserTest,
                       VizInputTransferTokenReceivedOnBrowser) {
  base::test::TestTraceProcessor ttp;
  ttp.StartTrace("input,Java");
  content::RenderFrameSubmissionObserver render_frame_submission_observer(
      shell()->web_contents());

  EXPECT_TRUE(content::NavigateToURL(
      shell(), GURL("data:text/html,<!doctype html>"
                    "<body style='background-color: magenta;'></body>")));
  if (render_frame_submission_observer.render_frame_count() == 0) {
    render_frame_submission_observer.WaitForAnyFrameSubmission();
  }
  // By this point a root compositor frame sink should have been created as
  // well, and if an input receiver were to be created it should have been
  // since it happens when root compositor frame sink is created.

  absl::Status status = ttp.StopAndParseTrace();
  EXPECT_TRUE(status.ok()) << status.message();

  std::string query = R"(SELECT COUNT(*) as cnt
                         FROM slice
                         WHERE slice.name = 'Storing InputTransferToken')";
  auto result = ttp.RunQuery(query);

  EXPECT_TRUE(result.has_value());

  // `result.value()` would look something like this: {{"cnt"}, {"<num>"}.
  EXPECT_EQ(result.value().size(), 2u);
  EXPECT_EQ(result.value()[1].size(), 1u);
  const std::string slice_count = result.value()[1][0];
  const std::string expected_count =
      InputUtils::IsTransferInputToVizSupported() ? "1" : "0";
  EXPECT_EQ(slice_count, expected_count);
}

// TODO(crbug.com/381039758): Re-enable after investigation.
IN_PROC_BROWSER_TEST_P(AndroidInputBrowserTest,
                       DISABLED_ReuseSingleInputReceiver) {
  base::HistogramTester histogram_tester;
  GURL url = GURL(
      "data:text/html,<!doctype html>"
      "<body style='background-color: magenta;'></body>");
  {
    content::RenderFrameSubmissionObserver render_frame_submission_observer(
        shell()->web_contents());
    EXPECT_TRUE(content::NavigateToURL(shell(), url));
    if (render_frame_submission_observer.render_frame_count() == 0) {
      render_frame_submission_observer.WaitForAnyFrameSubmission();
    }
  }

  auto* second_shell = content::Shell::CreateNewWindow(
      shell()->web_contents()->GetBrowserContext(), url, nullptr, {800, 600});
  {
    content::RenderFrameSubmissionObserver render_frame_submission_observer(
        second_shell->web_contents());
    if (render_frame_submission_observer.render_frame_count() == 0) {
      render_frame_submission_observer.WaitForAnyFrameSubmission();
    }
  }

  const int expected_count =
      InputUtils::IsTransferInputToVizSupported() ? 1 : 0;
  content::FetchHistogramsFromChildProcesses();
  histogram_tester.ExpectBucketCount(
      "Android.InputOnViz.InputReceiverCreationResult",
      /*kReuseExistingInputReceiver*/ 7, expected_count);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AndroidInputBrowserTest,
                         ::testing::Bool(),
                         [](auto& info) {
                           return info.param ? "InputOnViz_Enabled"
                                             : "InputOnViz_Disabled";
                         });
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace input
