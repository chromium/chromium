// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
#define CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;

// Implementation of ContentSettingBubbleModelDelegate which uses an instance of
// Browser in order to fulfil its duties.
class BrowserContentSettingBubbleModelDelegate
    : public ContentSettingBubbleModelDelegate {
 public:
  DECLARE_USER_DATA(BrowserContentSettingBubbleModelDelegate);

  explicit BrowserContentSettingBubbleModelDelegate(
      BrowserWindowInterface* browser);

  // Returns the delegate for `browser`, or null if it does not have one.
  static BrowserContentSettingBubbleModelDelegate* From(
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
  ui::ScopedUnownedUserData<BrowserContentSettingBubbleModelDelegate>
      scoped_unowned_user_data_;

  const raw_ref<BrowserWindowInterface> browser_;
};

#endif  // CHROME_BROWSER_UI_BROWSER_CONTENT_SETTING_BUBBLE_MODEL_DELEGATE_H_
