// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/flash_embed_rewrite.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(FlashEmbedRewriteTest, YouTubeRewriteEmbed) {
  struct TestData {
    std::string original;
    std::string expected;
  } test_data[] = {
      // { original, expected }
      {"http://youtube.com", ""},
      {"http://www.youtube.com", ""},
      {"https://www.youtube.com", ""},
      {"http://www.foo.youtube.com", ""},
      {"https://www.foo.youtube.com", ""},
      // Non-YouTube domains shouldn't be modified
      {"http://www.plus.google.com", ""},
      // URL isn't using Flash
      {"http://www.youtube.com/embed/deadbeef", ""},
      // URL isn't using Flash, no www
      {"http://youtube.com/embed/deadbeef", ""},
      // URL isn't using Flash, invalid parameter construct
      {"http://www.youtube.com/embed/deadbeef&start=4", ""},
      // URL is using Flash, no www
      {"http://youtube.com/v/deadbeef", "http://youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, https
      {"https://www.youtube.com/v/deadbeef",
       "https://www.youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, http
      {"http://www.youtube.com/v/deadbeef",
       "http://www.youtube.com/embed/deadbeef"},
      // URL is using Flash, valid
      {"https://www.foo.youtube.com/v/deadbeef",
       "https://www.foo.youtube.com/embed/deadbeef"},
      // URL is using Flash, is valid, has one parameter
      {"http://www.youtube.com/v/deadbeef?start=4",
       "http://www.youtube.com/embed/deadbeef?start=4"},
      // URL is using Flash, is valid, has multiple parameters
      {"http://www.youtube.com/v/deadbeef?start=4&fs=1",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1"},
      // URL is using Flash, invalid parameter construct, has one parameter
      {"http://www.youtube.com/v/deadbeef&start=4",
       "http://www.youtube.com/embed/deadbeef?start=4"},
      // URL is using Flash, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube.com/v/deadbeef&start=4&fs=1?foo=bar",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1&foo=bar"},
      // URL is using Flash, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube.com/v/deadbeef&start=4&fs=1",
       "http://www.youtube.com/embed/deadbeef?start=4&fs=1"},
      // Invalid parameter construct
      {"http://www.youtube.com/abcd/v/deadbeef", ""},
      // Invalid parameter construct
      {"http://www.youtube.com/v/abcd/", "http://www.youtube.com/embed/abcd/"},
      // Invalid parameter construct
      {"http://www.youtube.com/v/123/", "http://www.youtube.com/embed/123/"},
      // youtube-nocookie.com
      {"http://www.youtube-nocookie.com/v/123/",
       "http://www.youtube-nocookie.com/embed/123/"},
      // youtube-nocookie.com, isn't using flash
      {"http://www.youtube-nocookie.com/embed/123/", ""},
      // youtube-nocookie.com, has one parameter
      {"http://www.youtube-nocookie.com/v/123?start=foo",
       "http://www.youtube-nocookie.com/embed/123?start=foo"},
      // youtube-nocookie.com, has multiple parameters
      {"http://www.youtube-nocookie.com/v/123?start=foo&bar=baz",
       "http://www.youtube-nocookie.com/embed/123?start=foo&bar=baz"},
      // youtube-nocookie.com, invalid parameter construct, has one parameter
      {"http://www.youtube-nocookie.com/v/123&start=foo",
       "http://www.youtube-nocookie.com/embed/123?start=foo"},
      // youtube-nocookie.com, invalid parameter construct, has multiple
      // parameters
      {"http://www.youtube-nocookie.com/v/123&start=foo&bar=baz",
       "http://www.youtube-nocookie.com/embed/123?start=foo&bar=baz"},
      // youtube-nocookie.com, https
      {"https://www.youtube-nocookie.com/v/123/",
       "https://www.youtube-nocookie.com/embed/123/"},
      // URL isn't using Flash, has JS API enabled
      {"http://www.youtube.com/embed/deadbeef?enablejsapi=1", ""},
      // URL is using Flash, has JS API enabled
      {"http://www.youtube.com/v/deadbeef?enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1"},
      // youtube-nocookie.com, has JS API enabled
      {"http://www.youtube-nocookie.com/v/123?enablejsapi=1",
       "http://www.youtube-nocookie.com/embed/123?enablejsapi=1"},
      // ... with multiple parameters.
      {"http://www.youtube.com/v/deadbeef?enablejsapi=1&foo=2",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1&foo=2"},
      // URL is using Flash, has JS API enabled, invalid parameter construct
      {"http://www.youtube.com/v/deadbeef&enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1"},
      // ... with multiple parameters.
      {"http://www.youtube.com/v/deadbeef&enablejsapi=1&foo=2",
       "http://www.youtube.com/embed/deadbeef?enablejsapi=1&foo=2"},
      // URL is using Flash, has JS API enabled, invalid parameter construct,
      // has multiple parameters
      {"http://www.youtube.com/v/deadbeef&start=4&enablejsapi=1",
       "http://www.youtube.com/embed/deadbeef?start=4&enablejsapi=1"},
      // URL with a strange domain. crbug.com/1492427
      {"http://wrel=0&amp;coww.youtube.com/v/vxt_QAYSKLA", ""}};

  for (const auto& data : test_data) {
    EXPECT_EQ(GURL(data.expected),
              FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data.original)));
  }
}

TEST(FlashEmbedRewriteTest, DailymotionRewriteEmbed) {
  struct TestData {
    std::string original;
    std::string expected;
  } test_data[] = {
      // { original, expected }
      {"http://dailymotion.com", ""},
      {"http://www.dailymotion.com", ""},
      {"https://www.dailymotion.com", ""},
      {"http://www.foo.dailymotion.com", ""},
      {"https://www.foo.dailymotion.com", ""},
      // URL isn't using Flash
      {"http://www.dailymotion.com/embed/video/deadbeef", ""},
      // URL isn't using Flash, no www
      {"http://dailymotion.com/embed/video/deadbeef", ""},
      // URL isn't using Flash, invalid parameter construct
      {"http://www.dailymotion.com/embed/video/deadbeef&start=4", ""},
      // URL is using Flash, no www
      {"http://dailymotion.com/swf/deadbeef",
       "http://dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, https
      {"https://www.dailymotion.com/swf/deadbeef",
       "https://www.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, http
      {"http://www.dailymotion.com/swf/deadbeef",
       "http://www.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, valid
      {"https://www.foo.dailymotion.com/swf/deadbeef",
       "https://www.foo.dailymotion.com/embed/video/deadbeef"},
      // URL is using Flash, is valid, has one parameter
      {"http://www.dailymotion.com/swf/deadbeef?start=4",
       "http://www.dailymotion.com/embed/video/deadbeef?start=4"},
      // URL is using Flash, is valid, has multiple parameters
      {"http://www.dailymotion.com/swf/deadbeef?start=4&fs=1",
       "http://www.dailymotion.com/embed/video/deadbeef?start=4&fs=1"},
      // URL is using Flash, invalid parameter construct, has one parameter
      {"http://www.dailymotion.com/swf/deadbeef&start=4",
       "http://www.dailymotion.com/embed/video/deadbeef&start=4"},
      // Invalid URL.
      {"http://www.dailymotion.com/abcd/swf/deadbeef", ""},
      // Uses /swf/video/
      {"http://www.dailymotion.com/swf/video/deadbeef",
       "http://www.dailymotion.com/embed/video/deadbeef"}};

  for (const auto& data : test_data) {
    EXPECT_EQ(GURL(data.expected),
              FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data.original)));
  }
}

TEST(FlashEmbedRewriteTest, VimeoRewriteEmbed) {
  struct TestData {
    std::string original;
    std::string expected;
  } test_data[] = {
      // { original, expected }
      {"http://vimeo.com", ""},
      {"http://wwwvimeo.com", ""},
      {"https://www.vimeo.com", ""},
      {"http://www.foo.vimeo.com", ""},
      {"https://www.foo.vimeo.com", ""},
      // URL isn't using Flash.
      {"https://player.vimeo.com/video/deadbeef", ""},
      // URL isn't using Flash, different origin.
      {"https://vimeo.com/video/deadbeef", ""},
      // URL is using Flash, is valid, http
      {"http://vimeo.com/moogaloop.swf?clip_id=deadbeef",
       "http://player.vimeo.com/video/deadbeef"},
      // URL is using Flash, is valid, https
      {"https://vimeo.com/moogaloop.swf?clip_id=deadbeef",
       "https://player.vimeo.com/video/deadbeef"},
      // URL is using Flash, is valid, has multiple parameters
      {"https://vimeo.com/"
       "moogaloop.swf?clip_id=deadbeef&amp;server=vimeo.com&amp;color=00adef&"
       "amp;fullscreen=1",
       "https://player.vimeo.com/video/deadbeef"},
      // URL is using Flash, invalid parameter construct, has one parameter
      {"https://vimeo.com/moogaloop.swf&clip_id=deadbeef",
       "https://player.vimeo.com/video/deadbeef"},
      // URL is using Flash, multiple parameters, clip_id in the middle
      {"https://vimeo.com/"
       "moogaloop.swf?server=vimeo.com&amp;clip_id=deadbeef&amp;color=00adef&"
       "amp;fullscreen=1",
       "https://player.vimeo.com/video/deadbeef"},
      // URL is using Flash, multiple parameters, clip_id at the end
      {"https://vimeo.com/"
       "moogaloop.swf?server=vimeo.com&amp;color=00adef&amp;fullscreen=1?clip_"
       "id=deadbeef",
       "https://player.vimeo.com/video/deadbeef"},
      // Invalid URL.
      {"https://vimeo.com/?clip_id=deadbeef", ""}};

  for (const auto& data : test_data) {
    EXPECT_EQ(GURL(data.expected),
              FlashEmbedRewrite::RewriteFlashEmbedURL(GURL(data.original)));
  }
}
