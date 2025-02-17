// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/favicon_base/favicon_callback.h"
#include "content/public/browser/web_contents_observer.h"

using base::android::JavaParamRef;

namespace content {
class WebContents;
class Page;
}  // namespace content

namespace permissions {

class PermissionDialogDelegate;
class PermissionPromptAndroid;

class PermissionDialogJavaDelegate {
 public:
  explicit PermissionDialogJavaDelegate(
      PermissionPromptAndroid* permission_prompt);
  virtual ~PermissionDialogJavaDelegate();

  PermissionDialogJavaDelegate(const PermissionDialogJavaDelegate&) = delete;
  PermissionDialogJavaDelegate& operator=(const PermissionDialogJavaDelegate&) =
      delete;

  virtual void CreateJavaDelegate(content::WebContents* web_contents,
                                  PermissionDialogDelegate* owner);
  virtual void CreateDialog(content::WebContents* web_contents);
  void GetAndUpdateRequestingOriginFavicon(content::WebContents* web_contents);
  void OnRequestingOriginFaviconLoaded(
      const favicon_base::FaviconRawBitmapResult& favicon_result);

  virtual void DismissDialog();

  virtual void UpdateDialog();

  virtual void NotifyPermissionAllowed();

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;
  raw_ptr<PermissionPromptAndroid, DanglingUntriaged> permission_prompt_;

  // The task tracker for loading favicons.
  base::CancelableTaskTracker favicon_tracker_;
};

// Delegate class for displaying a permission prompt as a modal dialog. Used as
// the native to Java interface to allow Java to communicate the user's
// decision.
class PermissionDialogDelegate : public content::WebContentsObserver {
 public:
  PermissionDialogDelegate(
      content::WebContents* web_contents,
      PermissionPromptAndroid* permission_prompt,
      std::unique_ptr<PermissionDialogJavaDelegate> java_delegate);
  ~PermissionDialogDelegate() override;

  // The interface for creating a modal dialog when the PermissionRequestManager
  // is enabled.
  static std::unique_ptr<PermissionDialogDelegate> Create(
      content::WebContents* web_contents,
      PermissionPromptAndroid* permission_prompt);

  static std::unique_ptr<PermissionDialogDelegate> CreateForTesting(
      content::WebContents* web_contents,
      PermissionPromptAndroid* permission_prompt,
      std::unique_ptr<PermissionDialogJavaDelegate> java_delegate);

  PermissionDialogDelegate(const PermissionDialogDelegate&) = delete;
  PermissionDialogDelegate& operator=(const PermissionDialogDelegate&) = delete;

  // JNI methods.
  void Accept(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void AcceptThisTime(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Acknowledge(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Deny(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Dismissed(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 int dismissalType);
  void Resumed(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void SystemSettingsShown(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void SystemPermissionResolved(JNIEnv* env,
                                const JavaParamRef<jobject>& obj,
                                bool accepted);

  // Reset the java JNI object object. Called from Java once the permission
  // dialog has been responded to.
  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);
  bool IsJavaDelegateDestroyed() const { return !java_delegate_; }

  // Notify Java side to update content view of the dialog associated with this
  // object.
  void UpdateDialog();

  // Notify Java side that the permission has been allowed. It's basically the
  // end if a permission flow and Java side can perform next task, such as
  // update permission icon or showing next dialog.
  void NotifyPermissionAllowed();

 private:
  // On navigation or page destruction, hide the dialog.
  void DismissDialog();

  void DestroyJavaDelegate() { java_delegate_.reset(); }

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page&) override;
  void WebContentsDestroyed() override;

  base::android::ScopedJavaLocalRef<jstring> GetPositiveButtonText(
      JNIEnv* env,
      bool is_one_time) const;
  base::android::ScopedJavaLocalRef<jstring> GetNegativeButtonText(
      JNIEnv* env,
      bool is_one_time) const;
  base::android::ScopedJavaLocalRef<jstring> GetPositiveEphemeralButtonText(
      JNIEnv* env,
      bool is_one_time) const;
  // `permission_prompt_` owns and outlives this object, this is safe to use.
  raw_ptr<PermissionPromptAndroid> permission_prompt_;

  // The PermissionDialogJavaDelegate abstracts away JNI connectivity from
  // native to Java in order to facilicate unit testing.
  std::unique_ptr<PermissionDialogJavaDelegate> java_delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_
