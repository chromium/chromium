// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_ANDROID_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "third_party/jni_zero/jni_zero.h"

class ExclusiveAccessBubbleAndroid;

// Delegate of the ExclusiveAccessManagerAndroid
class ExclusiveAccessContextAndroid : public ExclusiveAccessContext {
 public:
  ExclusiveAccessContextAndroid(
      JNIEnv* env,
      const jni_zero::JavaRef<jobject>& j_context,
      const jni_zero::JavaRef<jobject>& j_fullscreen_manager,
      const jni_zero::JavaRef<jobject>& j_activity_tab_provider);
  ~ExclusiveAccessContextAndroid() override;

  void Destroy(JNIEnv* env);

  bool IsFullscreen() const override;

  Profile* GetProfile() override;

  // Called when we transition between tab and browser fullscreen. This method
  // updates the UI by showing/hiding the tab strip, toolbar and bookmark bar
  // in the browser fullscreen. Currently only supported on Mac.
  void UpdateUIForTabFullscreen() override {}

  // Enters fullscreen and updates the exclusive access bubble.
  void EnterFullscreen(const url::Origin& origin,
                       ExclusiveAccessBubbleType bubble_type,
                       FullscreenTabParams fullscreen_tab_params) override;

  // Exits fullscreen and updates the exclusive access bubble.
  void ExitFullscreen() override;

  // Updates the exclusive access bubble.
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;

  // Returns whether the exclusive access bubble is currently shown.
  bool IsExclusiveAccessBubbleDisplayed() const override;

  // Informs the exclusive access system of some user input, which may update
  // internal timers and/or re-display the bubble.
  void OnExclusiveAccessUserInput() override;

  // Returns the currently active WebContents, or nullptr if there is none.
  content::WebContents* GetWebContentsForExclusiveAccess() override;

  // window.setResizable(false) blocks user-initiated fullscreen requests, see:
  // https://github.com/explainers-by-googlers/additional-windowing-controls/blob/main/README.md
  bool CanUserEnterFullscreen() const override;

  // There are special modes where the user isn't allowed to exit fullscreen on
  // their own, and this function allows us to check for that.
  bool CanUserExitFullscreen() const override;

  void ForceActiveTab(JNIEnv* env, const jni_zero::JavaRef<jobject>& j_tab);

  base::WeakPtr<ExclusiveAccessContextAndroid> GetAsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void DestroyAnyExclusiveAccessBubble();

  base::android::ScopedJavaGlobalRef<jobject> java_context_;
  std::unique_ptr<ExclusiveAccessBubbleAndroid> exclusive_access_bubble_;
  base::CancelableTaskTracker exclusive_access_bubble_cancelable_task_tracker_;
  std::optional<base::CancelableTaskTracker::TaskId>
      exclusive_access_bubble_destruction_task_id_;
  base::WeakPtrFactory<ExclusiveAccessContextAndroid> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXCLUSIVE_ACCESS_EXCLUSIVE_ACCESS_CONTEXT_ANDROID_H_
