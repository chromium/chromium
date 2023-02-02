// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_started_animation.h"

#include "chrome/browser/ui/views/download/download_shelf_started_animation_views.h"

namespace content {
class WebContents;
}  // namespace content

// static
void DownloadStartedAnimation::Show(content::WebContents* web_contents) {
  // The animation will delete itself when it's finished.
  new DownloadShelfStartedAnimationViews(web_contents);
}
