// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/bluetooth_scanning_prompt_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "components/permissions/android/bluetooth_scanning_prompt_android_delegate.h"
#include "components/permissions/permission_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/BluetoothScanningPermissionDialog_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace permissions {

namespace {

BluetoothScanningPromptAndroid::CreateJavaDialogCallback
GetCreateJavaBluetoothScanningPromptCallback() {
  return base::BindOnce(&Java_BluetoothScanningPermissionDialog_create);
}

}  // namespace

BluetoothScanningPromptAndroid::BluetoothScanningPromptAndroid(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler,
    std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate,
    CreateJavaDialogCallback create_java_dialog_callback)
    : web_contents_(content::WebContents::FromRenderFrameHost(frame)),
      event_handler_(event_handler),
      delegate_(std::move(delegate)) {
  // Permission delegation means the permission request should be attributed to
  // the main frame.
  const url::Origin origin = url::Origin::Create(
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          frame->GetMainFrame()));
  DCHECK(!origin.opaque());

  ScopedJavaLocalRef<jobject> window_android =
      web_contents_->GetNativeView()->GetWindowAndroid()->GetJavaObject();

  // Create (and show) the BluetoothScanningPermission dialog.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> origin_string = ConvertUTF16ToJavaString(
      env, url_formatter::FormatUrlForSecurityDisplay(origin.GetURL()));
  java_dialog_.Reset(std::move(create_java_dialog_callback)
                         .Run(env, window_android, origin_string,
                              delegate_->GetSecurityLevel(web_contents_),
                              delegate_->GetJavaObject(),
                              reinterpret_cast<intptr_t>(this)));
}

BluetoothScanningPromptAndroid::BluetoothScanningPromptAndroid(
    content::RenderFrameHost* frame,
    const content::BluetoothScanningPrompt::EventHandler& event_handler,
    std::unique_ptr<BluetoothScanningPromptAndroidDelegate> delegate)
    : BluetoothScanningPromptAndroid(
          frame,
          event_handler,
          std::move(delegate),
          GetCreateJavaBluetoothScanningPromptCallback()) {}

BluetoothScanningPromptAndroid::~BluetoothScanningPromptAndroid() {
  if (!java_dialog_.is_null()) {
    Java_BluetoothScanningPermissionDialog_closeDialog(AttachCurrentThread(),
                                                       java_dialog_);
  }
}

void BluetoothScanningPromptAndroid::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      ConvertUTF8ToJavaString(env, device_id);
  ScopedJavaLocalRef<jstring> java_device_name =
      ConvertUTF16ToJavaString(env, device_name);
  Java_BluetoothScanningPermissionDialog_addOrUpdateDevice(
      env, java_dialog_, java_device_id, java_device_name);
}

void BluetoothScanningPromptAndroid::OnDialogFinished(JNIEnv* env,
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
  NOTREACHED_IN_MIGRATION();
}

}  // namespace permissions
