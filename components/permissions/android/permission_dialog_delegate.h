// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_DIALOG_DELEGATE_H_
#define COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_DIALOG_DELEGATE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/android/permission_prompt_android.h"
#include "components/permissions/permission_util.h"
#include "content/public/browser/web_contents_observer.h"

using base::android::JavaParamRef;

namespace content {
class WebContents;
}

namespace permissions {

// Delegate class for displaying a permission prompt as a modal dialog. Used as
// the native to Java interface to allow Java to communicate the user's
// decision.
class PermissionDialogDelegate : public content::WebContentsObserver {
 public:
  // The interface for creating a modal dialog when the PermissionRequestManager
  // is enabled.
  static void Create(content::WebContents* web_contents,
                     PermissionPromptAndroid* permission_prompt);

  // JNI methods.
  void Accept(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Cancel(JNIEnv* env, const JavaParamRef<jobject>& obj);
  void Dismissed(JNIEnv* env, const JavaParamRef<jobject>& obj);

  // Frees this object. Called from Java once the permission dialog has been
  // responded to.
  void Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj);

 private:
  PermissionDialogDelegate(content::WebContents* web_contents,
                           PermissionPromptAndroid* permission_prompt);
  ~PermissionDialogDelegate() override;

  void CreateJavaDelegate(JNIEnv* env, content::WebContents* web_contents);

  // On navigation or page destruction, hide the dialog.
  void DismissDialog();

  // WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void WebContentsDestroyed() override;

  base::android::ScopedJavaGlobalRef<jobject> j_delegate_;

  // The PermissionPromptAndroid is deleted when either the dialog is resolved
  // or the tab is navigated/closed. We close the prompt on DidFinishNavigation
  // and WebContentsDestroyed, so it should always be safe to use this pointer.
  PermissionPromptAndroid* permission_prompt_;

  DISALLOW_COPY_AND_ASSIGN(PermissionDialogDelegate);
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_ANDROID_PERMISSION_DIALOG_DELEGATE_H_
