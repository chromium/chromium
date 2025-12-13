// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_ANDROID_H_

#include <string>

#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_hide_callback.h"
#include "third_party/jni_zero/jni_zero.h"

// The android implementation of ExclusiveAccessBubble, this class is used to
// manage the exclusive access bubble for the APIs (fullscreen, keyboard lock &
// pointer lock), actual display logic is handled by the java side.
class ExclusiveAccessBubbleAndroid : public ExclusiveAccessBubble {
 public:
  ExclusiveAccessBubbleAndroid(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback,
      const jni_zero::ScopedJavaGlobalRef<jobject>& jcontext);

  ExclusiveAccessBubbleAndroid(const ExclusiveAccessBubbleAndroid&) = delete;
  ExclusiveAccessBubbleAndroid& operator=(const ExclusiveAccessBubbleAndroid&) =
      delete;

  ~ExclusiveAccessBubbleAndroid() override;

  // Updates and re/shows the exclusive access bubble based on the given params,
  // if the bubble is already shown with the same content, then it will not be
  // updated.
  void Update(const ExclusiveAccessBubbleParams& params,
              ExclusiveAccessBubbleHideCallback first_hide_callback);

  // If popup is visible, hides |popup_| before the bubble automatically hides
  // itself.
  void HideImmediately();

  // Returns whether the popup is visible.
  bool IsVisible() const;

 private:
  // ExclusiveAccessBubble:
  void Hide() override;
  void Show() override;

  void UpdateBubbleContent(ExclusiveAccessBubbleType bubble_type);
  void RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason reason);

  std::u16string GetBubbleText(ExclusiveAccessBubbleType bubble_type,
                               bool keyboard_connected) const;

  ExclusiveAccessBubbleParams params_;
  bool notify_overridden_;
  ExclusiveAccessBubbleHideCallback first_hide_callback_;
  std::u16string browser_fullscreen_exit_accelerator_;
  jni_zero::ScopedJavaGlobalRef<jobject> j_bubble_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_BUBBLE_ANDROID_H_
