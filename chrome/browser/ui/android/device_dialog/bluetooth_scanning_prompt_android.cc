// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/bluetooth_scanning_prompt_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/BluetoothScanningPermissionDialog_jni.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

BluetoothScanningPromptAndroid::BluetoothScanningPromptAndroid(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler)
    : web_contents_(content::WebContents::FromRenderFrameHost(frame)),
      event_handler_(event_handler) {
  const url::Origin origin = frame->GetLastCommittedOrigin();
  DCHECK(!origin.opaque());

  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents_->GetNativeView()->GetWindowAndroid()->GetJavaObject();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);

  // Create (and show) the BluetoothScanningPermission dialog.
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatUrlForSecurityDisplay(origin.GetURL()));
  java_dialog_.Reset(Java_BluetoothScanningPermissionDialog_create(
      env, window_android, origin_string, helper->GetSecurityLevel(),
      reinterpret_cast<intptr_t>(this)));
}

BluetoothScanningPromptAndroid::~BluetoothScanningPromptAndroid() {
  if (!java_dialog_.is_null()) {
    Java_BluetoothScanningPermissionDialog_closeDialog(AttachCurrentThread(),
                                                       java_dialog_);
  }
}

void BluetoothScanningPromptAndroid::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const base::string16& device_name) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      ConvertUTF8ToJavaString(env, device_id);
  ScopedJavaLocalRef<jstring> java_device_name =
      ConvertUTF16ToJavaString(env, device_name);
  Java_BluetoothScanningPermissionDialog_addOrUpdateDevice(
      env, java_dialog_, java_device_id, java_device_name);
}

void BluetoothScanningPromptAndroid::OnDialogFinished(
    JNIEnv* env,
    jint event_type) {
  // Values are defined in BluetoothScanningPromptDialog as DIALOG_FINISHED
  // constants.
  switch (event_type) {
    case 0:
      event_handler_.Run(content::BluetoothScanningPrompt::Event::kAllow);
      return;
    case 1:
      event_handler_.Run(content::BluetoothScanningPrompt::Event::kBlock);
      return;
    case 2:
      event_handler_.Run(content::BluetoothScanningPrompt::Event::kCanceled);
      return;
  }
  NOTREACHED();
}
