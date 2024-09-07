// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/android/app_modal_dialog_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_queue.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/javascript_dialog_type.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/javascript_dialogs/android/jni_headers/JavascriptAppModalDialog_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace javascript_dialogs {

AppModalDialogViewAndroid::AppModalDialogViewAndroid(
    JNIEnv* env,
    AppModalDialogController* controller,
    gfx::NativeWindow parent)
    : controller_(controller),
      parent_jobject_weak_ref_(env, parent->GetJavaObject()) {
  controller->web_contents()->GetDelegate()->ActivateContents(
      controller->web_contents());
}

void AppModalDialogViewAndroid::ShowAppModalDialog() {
  JNIEnv* env = AttachCurrentThread();
  // Keep a strong ref to the parent window while we make the call to java to
  // display the dialog.
  ScopedJavaLocalRef<jobject> parent_jobj = parent_jobject_weak_ref_.get(env);
  if (parent_jobj.is_null()) {
    CancelAppModalDialog();
    return;
  }

  ScopedJavaLocalRef<jobject> dialog_object;
  ScopedJavaLocalRef<jstring> title =
      ConvertUTF16ToJavaString(env, controller_->title());
  ScopedJavaLocalRef<jstring> message =
      ConvertUTF16ToJavaString(env, controller_->message_text());

  switch (controller_->javascript_dialog_type()) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      dialog_object = Java_JavascriptAppModalDialog_createAlertDialog(
          env, title, message, controller_->display_suppress_checkbox());
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      if (controller_->is_before_unload_dialog()) {
        dialog_object = Java_JavascriptAppModalDialog_createBeforeUnloadDialog(
            env, title, message, controller_->is_reload(),
            controller_->display_suppress_checkbox());
      } else {
        dialog_object = Java_JavascriptAppModalDialog_createConfirmDialog(
            env, title, message, controller_->display_suppress_checkbox());
      }
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> default_prompt_text =
          ConvertUTF16ToJavaString(env, controller_->default_prompt_text());
      dialog_object = Java_JavascriptAppModalDialog_createPromptDialog(
          env, title, message, controller_->display_suppress_checkbox(),
          default_prompt_text);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Keep a ref to the java side object until we get a confirm or cancel.
  dialog_jobject_.Reset(dialog_object);

  Java_JavascriptAppModalDialog_showJavascriptAppModalDialog(
      env, dialog_object, parent_jobj, reinterpret_cast<intptr_t>(this));
}

void AppModalDialogViewAndroid::ActivateAppModalDialog() {
  // This is called on desktop (Views) when interacting with a browser window
  // that does not host the currently active app modal dialog, as a way to
  // redirect activation to the app modal dialog host. It's not relevant on
  // Android.
  NOTREACHED_IN_MIGRATION();
}

void AppModalDialogViewAndroid::CloseAppModalDialog() {
  CancelAppModalDialog();
}

void AppModalDialogViewAndroid::AcceptAppModalDialog() {
  std::u16string prompt_text;
  controller_->OnAccept(prompt_text, false);
  delete this;
}

void AppModalDialogViewAndroid::DidAcceptAppModalDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const JavaParamRef<jstring>& prompt,
    bool should_suppress_js_dialogs) {
  std::u16string prompt_text =
      base::android::ConvertJavaStringToUTF16(env, prompt);
  controller_->OnAccept(prompt_text, should_suppress_js_dialogs);
  delete this;
}

void AppModalDialogViewAndroid::CancelAppModalDialog() {
  controller_->OnCancel(false);
  delete this;
}

bool AppModalDialogViewAndroid::IsShowing() const {
  return true;
}

void AppModalDialogViewAndroid::DidCancelAppModalDialog(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    bool should_suppress_js_dialogs) {
  controller_->OnCancel(should_suppress_js_dialogs);
  delete this;
}

const ScopedJavaGlobalRef<jobject>& AppModalDialogViewAndroid::GetDialogObject()
    const {
  return dialog_jobject_;
}

AppModalDialogViewAndroid::~AppModalDialogViewAndroid() {
  // In case the dialog is still displaying, tell it to close itself.
  // This can happen if you trigger a dialog but close the Tab before it's
  // shown, and then accept the dialog.
  if (!dialog_jobject_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_JavascriptAppModalDialog_dismiss(env, dialog_jobject_);
  }
}

// static
ScopedJavaLocalRef<jobject> JNI_JavascriptAppModalDialog_GetCurrentModalDialog(
    JNIEnv* env) {
  AppModalDialogController* controller =
      AppModalDialogQueue::GetInstance()->active_dialog();
  if (!controller || !controller->view())
    return ScopedJavaLocalRef<jobject>();

  AppModalDialogViewAndroid* js_dialog =
      static_cast<AppModalDialogViewAndroid*>(controller->view());
  return ScopedJavaLocalRef<jobject>(js_dialog->GetDialogObject());
}

}  // namespace javascript_dialogs
