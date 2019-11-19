// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/media/flash_embed_rewrite.h"

#include "base/metrics/histogram_macros.h"
#include "url/gurl.h"

GURL FlashEmbedRewrite::RewriteFlashEmbedURL(const GURL& url) {
  DCHECK(url.is_valid());

  if (url.DomainIs("youtube.com") || url.DomainIs("youtube-nocookie.com"))
    return RewriteYouTubeFlashEmbedURL(url);

  if (url.DomainIs("dailymotion.com"))
    return RewriteDailymotionFlashEmbedURL(url);

  return GURL();
}

GURL FlashEmbedRewrite::RewriteYouTubeFlashEmbedURL(const GURL& url) {
  // YouTube URLs are of the form of youtube.com/v/VIDEO_ID. So, we check to see
  // if the given URL does follow that format.
  if (url.path().find("/v/") != 0)
    return GURL();

  std::string url_str = url.spec();

  // If the website is using an invalid YouTube URL, we will try and
  // fix the URL by ensuring that if there are multiple parameters,
  // the parameter string begins with a "?" and then follows with a "&"
  // for each subsequent parameter. We do this because the Flash video player
  // has some URL correction capabilities so we don't want this move to HTML5
  // to break webpages that used to work.
  size_t index = url_str.find_first_of("&?");
  bool invalid_url = index != std::string::npos && url_str.at(index) == '&';

  if (invalid_url) {
    // ? should appear first before all parameters.
    url_str.replace(index, 1, "?");

    // Replace all instances of ? (after the first) with &.
    for (size_t pos = index + 1;
         (pos = url_str.find("?", pos)) != std::string::npos; pos += 1) {
      url_str.replace(pos, 1, "&");
    }
  }

  GURL corrected_url = GURL(url_str);

  // Change the path to use the YouTube HTML5 API.
  std::string path = corrected_url.path();
  path.replace(path.find("/v/"), 3, "/embed/");

  url::Replacements<char> r;
  r.SetPath(path.c_str(), url::Component(0, path.length()));

  return corrected_url.ReplaceComponents(r);
}

GURL FlashEmbedRewrite::RewriteDailymotionFlashEmbedURL(const GURL& url) {
  // Dailymotion flash embeds are of the form of either:
  //  - /swf/
  //  - /swf/video/
  if (url.path().find("/swf/") != 0)
    return GURL();

  std::string path = url.path();
  int replace_length = path.find("/swf/video/") == 0 ? 11 : 5;
  path.replace(0, replace_length, "/embed/video/");

  url::Replacements<char> r;
  r.SetPath(path.c_str(), url::Component(0, path.length()));

  return url.ReplaceComponents(r);
}
