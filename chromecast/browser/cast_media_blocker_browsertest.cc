// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromecast/browser/cast_media_blocker.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {

class CastMediaBlockerBrowserTest : public CastBrowserTest {
 public:
  CastMediaBlockerBrowserTest() {}

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

    std::string query = media::GetURLQueryString(query_params);
    GURL gurl = content::GetFileUrlWithQuery(
        media::GetTestDataFilePath("player.html"), query);

    web_contents_ = NavigateToURL(gurl);
    WaitForLoadStop(web_contents_);

    blocker_ = std::make_unique<CastMediaBlocker>(web_contents_);
  }

  void BlockAndTestPlayerState(const std::string& media_type, bool blocked) {
    blocker_->BlockMediaLoading(blocked);

    // Changing states is not instant, but should be timely (< 0.5s).
    for (size_t i = 0; i < 5; i++) {
      LOG(INFO) << "Checking media blocking, re-try = " << i;
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(100));
      run_loop.Run();

      const std::string command =
          "document.getElementsByTagName(\"" + media_type + "\")[0].paused";
      const std::string js =
          "window.domAutomationController.send(" + command + ");";

      bool paused;
      ASSERT_TRUE(ExecuteScriptAndExtractBool(web_contents_, js, &paused));

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

  DISALLOW_COPY_AND_ASSIGN(CastMediaBlockerBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CastMediaBlockerBrowserTest, Audio_BlockUnblock) {
  PlayMedia("audio", "bear-audio-10s-CBR-has-TOC.mp3");

  BlockAndTestPlayerState("audio", true);
  BlockAndTestPlayerState("audio", false);
}

#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
IN_PROC_BROWSER_TEST_F(CastMediaBlockerBrowserTest, Video_BlockUnblock) {
  PlayMedia("video", "tulip2.webm");

  BlockAndTestPlayerState("video", true);
  BlockAndTestPlayerState("video", false);
}
#endif

}  // namespace shell
}  // namespace chromecast
