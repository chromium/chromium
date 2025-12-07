// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/platform_thread.h"
#include "chromecast/browser/cast_media_blocker.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {

// TODO(crbug.com/40120884): Move relevant tests to components/browsertests so
// there is common coverage of MediaBlocker across platforms.
class CastMediaBlockerBrowserTest : public CastBrowserTest {
 protected:
  // CastBrowserTest implementation.
  void TearDownOnMainThread() override {
    blocker_.reset();

    CastBrowserTest::TearDownOnMainThread();
  }

  void PlayMedia(const std::string& tag, const std::string& media_file) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair(tag, media_file));
    query_params.push_back(std::make_pair("loop", "true"));

    std::string query = ::media::GetURLQueryString(query_params);
    GURL gurl = content::GetFileUrlWithQuery(
        ::media::GetTestDataFilePath("player.html"), query);

    web_contents_ = NavigateToURL(gurl);
    EXPECT_TRUE(WaitForLoadStop(web_contents_));

    blocker_ = std::make_unique<CastMediaBlocker>(web_contents_);
  }

  void BlockAndTestPlayerState(const std::string& media_type, bool blocked) {
    blocker_->BlockMediaLoading(blocked);

    // Changing states is not instant, but should be timely (< 0.5s).
    for (size_t i = 0; i < 5; i++) {
      LOG(INFO) << "Checking media blocking, re-try = " << i;
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(100));
      run_loop.Run();

      const std::string command =
          "document.getElementsByTagName(\"" + media_type + "\")[0].paused";

      bool paused = EvalJs(web_contents_, command).ExtractBool();

      if (paused == blocked) {
        SUCCEED() << "Media element has been successfullly "
                  << (blocked ? "blocked" : "unblocked");
        return;
      }
    }

    FAIL() << "Could not successfullly " << (blocked ? "block" : "unblock")
           << " media element";
  }

 private:
  content::WebContents* web_contents_;
  std::unique_ptr<CastMediaBlocker> blocker_;
};

// TODO(b/341792190): Re-enable tests.
IN_PROC_BROWSER_TEST_F(CastMediaBlockerBrowserTest,
                       DISABLED_Audio_BlockUnblock) {
  PlayMedia("audio", "bear-audio-10s-CBR-has-TOC.mp3");

  BlockAndTestPlayerState("audio", true);
  BlockAndTestPlayerState("audio", false);
}

#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
// TODO(b/341792190): Re-enable tests.
IN_PROC_BROWSER_TEST_F(CastMediaBlockerBrowserTest,
                       DISABLED_Video_BlockUnblock) {
  PlayMedia("video", "tulip2.webm");

  BlockAndTestPlayerState("video", true);
  BlockAndTestPlayerState("video", false);
}
#endif

}  // namespace shell
}  // namespace chromecast
