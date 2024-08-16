// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "base/test/test_trace_processor.h"
#include "components/input/features.h"
#include "components/input/utils.h"
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
  const bool expected_creation = TransferInputToViz() ? true : false;
  EXPECT_EQ(IsRenderInputRouterCreatedOnViz(), expected_creation);
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
