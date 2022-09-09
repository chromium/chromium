// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_

#include "components/content_settings/core/common/content_settings_types.h"

namespace content {
class WebContents;
}

// Delegate which is used by ContentSettingBubbleModel class.
class ContentSettingBubbleModelDelegate {
 public:
  // Shows the cookies collected in the web contents.
  virtual void ShowCollectedCookiesDialog(
      content::WebContents* web_contents) = 0;

  // Shows the Content Settings page for a given content type.
  virtual void ShowContentSettingsPage(ContentSettingsType type) = 0;

  // Shows the settings page for media.
  virtual void ShowMediaSettingsPage() = 0;

  // Shows the Learn More page for a given content type.
  virtual void ShowLearnMorePage(ContentSettingsType type) = 0;

 protected:
  virtual ~ContentSettingBubbleModelDelegate() {}
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
