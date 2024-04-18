// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/task/cancelable_task_tracker.h"
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
  // The interface for creating a modal dialog when the PermissionRequestManager
  // is enabled.
  static void Create(content::WebContents* web_contents,
                     PermissionPromptAndroid* permission_prompt);

  static PermissionDialogDelegate* CreateForTesting(
      content::WebContents* web_contents,
      PermissionPromptAndroid* permission_prompt,
      std::unique_ptr<PermissionDialogJavaDelegate> java_delegate);

  PermissionDialogDelegate(const PermissionDialogDelegate&) = delete;
  PermissionDialogDelegate& operator=(const PermissionDialogDelegate&) = delete;

  // JNI methods.
  void Accept(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void AcceptThisTime(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Cancel(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Dismissed(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 int dismissalType);

  // Frees this object. Called from Java once the permission dialog has been
  // responded to.
  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);

 private:
  PermissionDialogDelegate(
      content::WebContents* web_contents,
      PermissionPromptAndroid* permission_prompt,
      std::unique_ptr<PermissionDialogJavaDelegate> java_delegate);
  ~PermissionDialogDelegate() override;

  // On navigation or page destruction, hide the dialog.
  void DismissDialog();

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page&) override;
  void WebContentsDestroyed() override;

  // The PermissionPromptAndroid is deleted when either the dialog is resolved
  // or the tab is navigated/closed. We close the prompt on DidFinishNavigation
  // and WebContentsDestroyed, so it should always be safe to use this pointer.
  raw_ptr<PermissionPromptAndroid, DanglingUntriaged> permission_prompt_;

  // The PermissionDialogJavaDelegate abstracts away JNI connectivity from
  // native to Java in order to facilicate unit testing.
  std::unique_ptr<PermissionDialogJavaDelegate> java_delegate_;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_PROMPT_PERMISSION_DIALOG_DELEGATE_H_
