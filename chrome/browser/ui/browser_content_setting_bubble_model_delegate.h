// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"

class BrowserWindowInterface;

// Implementation of ContentSettingBubbleModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserContentSettingBubbleModelDelegate
    : public ContentSettingBubbleModelDelegate {
 public:
  explicit BrowserContentSettingBubbleModelDelegate(
      BrowserWindowInterface* browser);

  BrowserContentSettingBubbleModelDelegate(
      const BrowserContentSettingBubbleModelDelegate&) = delete;
  BrowserContentSettingBubbleModelDelegate& operator=(
      const BrowserContentSettingBubbleModelDelegate&) = delete;

  ~BrowserContentSettingBubbleModelDelegate() override;

  // ContentSettingBubbleModelDelegate implementation:
  void ShowCollectedCookiesDialog(content::WebContents* web_contents) override;
  void ShowContentSettingsPage(ContentSettingsType type) override;
  void ShowMediaSettingsPage() override;
  void ShowLearnMorePage(ContentSettingsType type) override;

 private:
  const raw_ref<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
