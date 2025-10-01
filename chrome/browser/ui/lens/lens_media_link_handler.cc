// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_media_link_handler.h"

#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"

namespace lens {

namespace {
// Query parameter for denoting a search companion request.
inline constexpr char kYoutubeHost[] = "www.youtube.com";
}  // namespace

LensMediaLinkHandler::LensMediaLinkHandler(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

LensMediaLinkHandler::~LensMediaLinkHandler() = default;

bool LensMediaLinkHandler::MaybeReplaceNavigation(const GURL& target) {
  auto* media_session = GetMediaSessionIfExists();
  const GURL& page_url = web_contents()->GetLastCommittedURL();
  if (!media_session || target.GetHost() != kYoutubeHost) {
    return false;
  }

  // Get the video ID and timestamp from the navigation URL.
  auto target_video_id = ExtractVideoNameIfExists(target);
  auto target_time = ExtractTimeInSecondsFromQueryIfExists(target);

  // Only proceed if the navigation URL contains a video ID and a time.
  if (!target_video_id || !target_time) {
    return false;
  }

  // Prioritize the video in the routed frame (for embeds).
  std::optional<std::string> source_video_id;
  if (auto* rfh = media_session->GetRoutedFrame()) {
    source_video_id = ExtractVideoNameIfExists(rfh->GetLastCommittedURL());
  }

  // If no embed is found, fall back to the main page's URL.
  if (!source_video_id && page_url.GetHost() == kYoutubeHost) {
    source_video_id = ExtractVideoNameIfExists(page_url);
  }

  // If the video playing matches the navigation target, seek to the new time.
  if (source_video_id && source_video_id == target_video_id) {
    media_session->SeekTo(*target_time);
    return true;
  }

  return false;
}

content::MediaSession* LensMediaLinkHandler::GetMediaSessionIfExists() {
  return content::MediaSession::GetIfExists(web_contents());
}

}  // namespace lens
