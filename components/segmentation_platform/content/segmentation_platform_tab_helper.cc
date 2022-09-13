// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/content/segmentation_platform_tab_helper.h"

#include "components/segmentation_platform/content/page_load_trigger_context.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace segmentation_platform {

SegmentationPlatformTabHelper::SegmentationPlatformTabHelper(
    content::WebContents* web_contents,
    SegmentationPlatformService* segmentation_platform_service)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<SegmentationPlatformTabHelper>(
          *web_contents),
      segmentation_platform_service_(segmentation_platform_service) {}

SegmentationPlatformTabHelper::~SegmentationPlatformTabHelper() = default;

void SegmentationPlatformTabHelper::PrimaryPageChanged(content::Page& page) {
  if (!segmentation_platform_service_)
    return;

  if (page.GetMainDocument().IsErrorDocument())
    return;

  // Only trigger for the visible tabs.
  if (GetWebContents().GetVisibility() == content::Visibility::HIDDEN)
    return;

  // This class will be deleted soon.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SegmentationPlatformTabHelper);

}  // namespace segmentation_platform
