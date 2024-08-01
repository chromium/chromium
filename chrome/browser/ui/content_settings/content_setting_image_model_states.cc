// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/content_settings/content_setting_image_model_states.h"

#include "base/check_op.h"

ContentSettingImageModelStates::~ContentSettingImageModelStates() = default;

// static
ContentSettingImageModelStates* ContentSettingImageModelStates::Get(
    content::WebContents* contents) {
  if (auto* state = FromWebContents(contents))
    return state;
  CreateForWebContents(contents);
  return FromWebContents(contents);
}

void ContentSettingImageModelStates::SetAnimationHasRun(
    ImageType type,
    bool animation_has_run) {
  VerifyType(type);
  animations_[static_cast<int>(type)] = animation_has_run;
}

bool ContentSettingImageModelStates::AnimationHasRun(ImageType type) const {
  VerifyType(type);
  return animations_[static_cast<int>(type)];
}

void ContentSettingImageModelStates::SetAccessibilityNotified(ImageType type,
                                                              bool notified) {
  VerifyType(type);

  accessibility_notified_[static_cast<int>(type)] = notified;
}

bool ContentSettingImageModelStates::GetAccessibilityNotified(
    ImageType type) const {
  VerifyType(type);

  return accessibility_notified_[static_cast<int>(type)];
}

void ContentSettingImageModelStates::SetBubbleWasAutoOpened(
    ImageType type,
    bool bubble_was_auto_opened) {
  VerifyType(type);
  auto_opened_bubbles_[static_cast<int>(type)] = bubble_was_auto_opened;
}

bool ContentSettingImageModelStates::BubbleWasAutoOpened(ImageType type) const {
  VerifyType(type);
  return auto_opened_bubbles_[static_cast<int>(type)];
}

void ContentSettingImageModelStates::SetPromoWasShown(ImageType type,
                                                      bool promo_was_shown) {
  VerifyType(type);
  promo_was_shown_[static_cast<int>(type)] = promo_was_shown;
}

bool ContentSettingImageModelStates::PromoWasShown(ImageType type) const {
  VerifyType(type);
  return promo_was_shown_[static_cast<int>(type)];
}

ContentSettingImageModelStates::ContentSettingImageModelStates(
    content::WebContents* contents)
    : content::WebContentsUserData<ContentSettingImageModelStates>(*contents) {}

void ContentSettingImageModelStates::VerifyType(ImageType type) const {
  CHECK_GE(type, static_cast<ImageType>(0));
  CHECK_LT(type, ImageType::NUM_IMAGE_TYPES);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContentSettingImageModelStates);
