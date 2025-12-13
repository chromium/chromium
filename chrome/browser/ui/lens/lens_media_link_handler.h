// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_LENS_LENS_MEDIA_LINK_HANDLER_H_
#define CHROME_BROWSER_UI_LENS_LENS_MEDIA_LINK_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace content {
class MediaSession;
class WebContents;
}  // namespace content

namespace lens {

// This class is intended to handle media links that are clicked while the Lens
// overlay is open. The primary responsibility is to intercept links to other
// videos on YouTube and seek to the appropriate timestamp before playing the
// video. This class should be constructed per-navigation and does not store
// state between navigations.
class LensMediaLinkHandler {
 public:
  explicit LensMediaLinkHandler(content::WebContents* web_contents);
  virtual ~LensMediaLinkHandler();

  // Called when the user clicks a link to `target` while `web_contents` is the
  // focused tab. Returns true if the navigation should be skipped due to
  // having a media session that should be seeked instead, false if it should be
  // allowed to proceed. This function does not actually modify or replace
  // the navigation.
  bool MaybeReplaceNavigation(const GURL& target);

  content::WebContents* web_contents() { return web_contents_; }

  // Use this instead of MediaSession::GetIfExists() to make tests easier.
  virtual content::MediaSession* GetMediaSessionIfExists();

 private:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

}  // namespace lens

#endif  // CHROME_BROWSER_UI_LENS_LENS_MEDIA_LINK_HANDLER_H_
