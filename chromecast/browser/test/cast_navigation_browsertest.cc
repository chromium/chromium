// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "chromecast/chromecast_buildflags.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/test_data_util.h"
#include "media/cdm/clear_key_cdm_common.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace chromecast {
namespace shell {
namespace {
const char16_t kEnded[] = u"ENDED";
const char16_t kError[] = u"ERROR";
const char16_t kFailed[] = u"FAILED";

const char kWebMAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
}

class CastNavigationBrowserTest : public CastBrowserTest {
 public:
  CastNavigationBrowserTest() {}

  CastNavigationBrowserTest(const CastNavigationBrowserTest&) = delete;
  CastNavigationBrowserTest& operator=(const CastNavigationBrowserTest&) =
      delete;

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory(
        ::media::GetTestDataPath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void LoadAboutBlank() {
    content::WebContents* web_contents =
        NavigateToURL(GURL(url::kAboutBlankURL));
    content::TitleWatcher title_watcher(web_contents, url::kAboutBlankURL16);
    std::u16string result = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(url::kAboutBlankURL16, result);
  }
  void PlayAudio(const std::string& media_file) {
    PlayMedia("audio", media_file);
  }
  void PlayVideo(const std::string& media_file) {
    PlayMedia("video", media_file);
  }
  void PlayEncryptedMedia(const std::string& key_system,
                          const std::string& media_type,
                          const std::string& media_file) {
    base::StringPairs query_params;
    query_params.emplace_back("mediaFile", media_file);
    query_params.emplace_back("mediaType", media_type);
    query_params.emplace_back("keySystem", key_system);
    query_params.emplace_back("useMSE", "1");

    RunMediaTestPage("eme_player.html", query_params, kEnded);
  }

 private:
  void PlayMedia(const std::string& tag, const std::string& media_file) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair(tag, media_file));
    RunMediaTestPage("player.html", query_params, kEnded);
  }

  void RunMediaTestPage(const std::string& html_page,
                        const base::StringPairs& query_params,
                        const std::u16string& expected_title) {
    std::string query = ::media::GetURLQueryString(query_params);
    GURL gurl = embedded_test_server()->GetURL("/" + html_page + "?" + query);
    std::u16string final_title = RunTest(gurl, expected_title);
    EXPECT_EQ(expected_title, final_title);
  }

  std::u16string RunTest(const GURL& gurl,
                         const std::u16string& expected_title) {
    content::WebContents* web_contents = NavigateToURL(gurl);
    content::TitleWatcher title_watcher(web_contents, expected_title);
    title_watcher.AlsoWaitForTitle(kEnded);
    title_watcher.AlsoWaitForTitle(kError);
    title_watcher.AlsoWaitForTitle(kFailed);
    return title_watcher.WaitAndGetTitle();
  }
};

IN_PROC_BROWSER_TEST_F(CastNavigationBrowserTest, EmptyTest) {
  // Run an entire browser lifecycle to ensure nothing breaks.
  LoadAboutBlank();
}

// Disabled due to flakiness. See crbug.com/813481.
IN_PROC_BROWSER_TEST_F(CastNavigationBrowserTest,
                       DISABLED_AudioPlaybackWavPcm) {
  PlayAudio("bear_pcm.wav");
}

#if !BUILDFLAG(IS_CAST_AUDIO_ONLY)
// TODO: reenable test; crashes occasionally: http://crbug.com/754269
IN_PROC_BROWSER_TEST_F(CastNavigationBrowserTest, DISABLED_VideoPlaybackMp4) {
  PlayVideo("bear.mp4");
}
#endif

IN_PROC_BROWSER_TEST_F(CastNavigationBrowserTest, ClearKeySupport) {
  PlayEncryptedMedia(media::kClearKeyKeySystem, kWebMAudioOnly,
                     "bear-a_enc-a.webm");
}

}  // namespace shell
}  // namespace chromecast
