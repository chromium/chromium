// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_SEGMENTATION_PLATFORM_TAB_HELPER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_SEGMENTATION_PLATFORM_TAB_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class Page;
class WebContents;
}  // namespace content

namespace segmentation_platform {
class SegmentationPlatformService;

// Observes navigation specific trigger events for a given tab.
class SegmentationPlatformTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<SegmentationPlatformTabHelper> {
 public:
  SegmentationPlatformTabHelper(
      content::WebContents* web_contents,
      SegmentationPlatformService* segmentation_platform_service);

  SegmentationPlatformTabHelper(const SegmentationPlatformTabHelper&) = delete;
  SegmentationPlatformTabHelper& operator=(
      const SegmentationPlatformTabHelper&) = delete;

  ~SegmentationPlatformTabHelper() override;

 private:
  friend class content::WebContentsUserData<SegmentationPlatformTabHelper>;

  // content::WebContentsObserver implementation
  void PrimaryPageChanged(content::Page& page) override;

  raw_ptr<SegmentationPlatformService> segmentation_platform_service_;
  base::WeakPtrFactory<SegmentationPlatformTabHelper> weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_CONTENT_SEGMENTATION_PLATFORM_TAB_HELPER_H_
