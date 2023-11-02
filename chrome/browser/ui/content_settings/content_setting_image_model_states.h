// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_STATES_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_STATES_H_

#include "chrome/browser/ui/content_settings/content_setting_image_model.h"
#include "content/public/browser/web_contents_user_data.h"

using ImageType = ContentSettingImageModel::ImageType;

// Class that keeps track of the tab-specific state associated with each
// ContentSettingImageModel. Each tab will have one instance of this class,
// which keeps track of states for each ImageType.
class ContentSettingImageModelStates
    : public content::WebContentsUserData<ContentSettingImageModelStates> {
 public:
  ContentSettingImageModelStates(const ContentSettingImageModelStates&) =
      delete;
  ContentSettingImageModelStates& operator=(
      const ContentSettingImageModelStates&) = delete;

  ~ContentSettingImageModelStates() override;

  static ContentSettingImageModelStates* Get(content::WebContents* contents);

  void SetAnimationHasRun(ImageType type, bool animation_has_run);
  bool AnimationHasRun(ImageType type) const;

  void SetAccessibilityNotified(ImageType type, bool notified);
  bool GetAccessibilityNotified(ImageType type) const;

  void SetBubbleWasAutoOpened(ImageType type, bool animation_has_run);
  bool BubbleWasAutoOpened(ImageType type) const;

  void SetPromoWasShown(ImageType type, bool promo_was_shown);
  bool PromoWasShown(ImageType type) const;

 private:
  friend class content::WebContentsUserData<ContentSettingImageModelStates>;
  explicit ContentSettingImageModelStates(content::WebContents* contents);

  // ImageTypes are used for direct access into a raw array, use this method to
  // CHECK that everything is in-bounds.
  void VerifyType(ImageType type) const;

  // Array of bool for whether an animation has run for a given image model.
  // This bit is reset to false when the image is hidden.
  bool animations_[static_cast<int>(ImageType::NUM_IMAGE_TYPES)] = {};

  // Array of bool for whether accessibility has been notified when an image
  // needs to be read out. Bit is stored per image type. This bit is reset to
  // false when the image is hidden.
  bool accessibility_notified_[static_cast<int>(ImageType::NUM_IMAGE_TYPES)] =
      {};

  // Array of bool for whether the bubble was auto-opened for a given image
  // model. This bit is reset to false when the image is hidden.
  bool auto_opened_bubbles_[static_cast<int>(ImageType::NUM_IMAGE_TYPES)] = {};

  // Array of bool for whether the indicator had a promo shown for a image
  // model. This bit is reset to false when the image is hidden.
  bool promo_was_shown_[static_cast<int>(ImageType::NUM_IMAGE_TYPES)] = {};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_STATES_H_
