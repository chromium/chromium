// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/android/tab_modal_dialog_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/android/window_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/javascript_dialogs/android/jni_headers/JavascriptTabModalDialog_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace javascript_dialogs {

// static
base::WeakPtr<TabModalDialogViewAndroid> TabModalDialogViewAndroid::Create(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled) {
  return (new TabModalDialogViewAndroid(
              parent_web_contents, alerting_web_contents, title, dialog_type,
              message_text, default_prompt_text,
              std::move(callback_on_button_clicked),
              std::move(callback_on_cancelled)))
      ->weak_factory_.GetWeakPtr();
}

// TabModalDialogViewAndroid:
TabModalDialogViewAndroid::~TabModalDialogViewAndroid() {
  // In case the dialog is still displaying, tell it to close itself.
  // This can happen if you trigger a dialog but close the Tab before it's
  // shown, and then accept the dialog.
  if (!dialog_jobject_.is_null()) {
    Java_JavascriptTabModalDialog_dismiss(AttachCurrentThread(),
                                          dialog_jobject_);
  }
}

void TabModalDialogViewAndroid::CloseDialogWithoutCallback() {
  delete this;
}

std::u16string TabModalDialogViewAndroid::GetUserInput() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> prompt =
      Java_JavascriptTabModalDialog_getUserInput(env, dialog_jobject_);
  return base::android::ConvertJavaStringToUTF16(env, prompt);
}

void TabModalDialogViewAndroid::Accept(JNIEnv* env,
                                       const JavaParamRef<jobject>&,
                                       const JavaParamRef<jstring>& prompt) {
  if (callback_on_button_clicked_) {
    std::u16string prompt_text =
        base::android::ConvertJavaStringToUTF16(env, prompt);
    std::move(callback_on_button_clicked_).Run(true, prompt_text);
  }
  delete this;
}

void TabModalDialogViewAndroid::Cancel(JNIEnv* env,
                                       const JavaParamRef<jobject>&,
                                       jboolean button_clicked) {
  if (button_clicked) {
    if (callback_on_button_clicked_) {
      std::move(callback_on_button_clicked_).Run(false, std::u16string());
    }
  } else if (callback_on_cancelled_) {
    std::move(callback_on_cancelled_).Run();
  }
  delete this;
}

TabModalDialogViewAndroid::TabModalDialogViewAndroid(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback
        callback_on_button_clicked,
    base::OnceClosure callback_on_cancelled)
    : callback_on_button_clicked_(std::move(callback_on_button_clicked)),
      callback_on_cancelled_(std::move(callback_on_cancelled)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  JNIEnv* env = AttachCurrentThread();
  jwindow_weak_ref_ = JavaObjectWeakGlobalRef(
      env, parent_web_contents->GetTopLevelNativeWindow()->GetJavaObject());

  // Keep a strong ref to the parent window while we make the call to java to
  // display the dialog.
  ScopedJavaLocalRef<jobject> jwindow = jwindow_weak_ref_.get(env);

  ScopedJavaLocalRef<jobject> dialog_object;
  ScopedJavaLocalRef<jstring> title_ref = ConvertUTF16ToJavaString(env, title);
  ScopedJavaLocalRef<jstring> message_ref =
      ConvertUTF16ToJavaString(env, message_text);

  switch (dialog_type) {
    case content::JAVASCRIPT_DIALOG_TYPE_ALERT: {
      dialog_object = Java_JavascriptTabModalDialog_createAlertDialog(
          env, title_ref, message_ref);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_CONFIRM: {
      dialog_object = Java_JavascriptTabModalDialog_createConfirmDialog(
          env, title_ref, message_ref);
      break;
    }
    case content::JAVASCRIPT_DIALOG_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> default_prompt_text_ref =
          ConvertUTF16ToJavaString(env, default_prompt_text);
      dialog_object = Java_JavascriptTabModalDialog_createPromptDialog(
          env, title_ref, message_ref, default_prompt_text_ref);
      break;
    }
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // Keep a ref to the java side object until we get accept or cancel.
  dialog_jobject_.Reset(dialog_object);

  Java_JavascriptTabModalDialog_showDialog(env, dialog_object, jwindow,
                                           reinterpret_cast<intptr_t>(this));
}

}  // namespace javascript_dialogs
