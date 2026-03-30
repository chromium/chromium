// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_VIEW_DELEGATE_H_

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"

namespace content {
class WebContents;
}

class ContentSettingBubbleModelDelegate;

class ContentSettingImageViewDelegate {
 public:
  // Delegate should return true if the content setting icon should be hidden.
  virtual bool ShouldHideContentSettingImage() = 0;

  // Gets the web contents the ContentSettingImageView is for.
  virtual content::WebContents* GetContentSettingWebContents() = 0;

  // Gets the ContentSettingBubbleModelDelegate for this
  // ContentSettingImageView.
  virtual ContentSettingBubbleModelDelegate*
  GetContentSettingBubbleModelDelegate() = 0;

  // Invoked when a bubble is shown.
  virtual void OnContentSettingImageBubbleShown(
      ContentSettingImageModel::ImageType type) const {}

 protected:
  virtual ~ContentSettingImageViewDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_VIEW_DELEGATE_H_
