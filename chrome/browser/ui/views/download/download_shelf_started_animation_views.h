// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_STARTED_ANIMATION_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_STARTED_ANIMATION_VIEWS_H_

#include "chrome/browser/ui/views/download/download_started_animation_views.h"
#include "ui/base/metadata/metadata_header_macros.h"

class DownloadShelfStartedAnimationViews
    : public DownloadStartedAnimationViews {
  METADATA_HEADER(DownloadShelfStartedAnimationViews,
                  DownloadStartedAnimationViews)

 public:
  explicit DownloadShelfStartedAnimationViews(
      content::WebContents* web_contents);
  DownloadShelfStartedAnimationViews(
      const DownloadShelfStartedAnimationViews&) = delete;
  DownloadShelfStartedAnimationViews& operator=(
      const DownloadShelfStartedAnimationViews&) = delete;
  ~DownloadShelfStartedAnimationViews() override = default;

 private:
  // DownloadStartedAnimationViews
  int GetX() const override;
  int GetY() const override;
  float GetOpacity() const override;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_SHELF_STARTED_ANIMATION_VIEWS_H_
