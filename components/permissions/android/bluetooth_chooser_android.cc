// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/android/bluetooth_chooser_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/permissions/android/bluetooth_chooser_android_delegate.h"
#include "components/permissions/constants.h"
#include "components/permissions/permission_util.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/permissions/android/jni_headers/BluetoothChooserDialog_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace permissions {

namespace {

BluetoothChooserAndroid::CreateJavaDialogCallback
GetCreateJavaBluetoothChooserDialogCallback() {
  return base::BindOnce(&Java_BluetoothChooserDialog_create);
}

}  // namespace

BluetoothChooserAndroid::BluetoothChooserAndroid(
    content::RenderFrameHost* frame,
    const EventHandler& event_handler,
    std::unique_ptr<BluetoothChooserAndroidDelegate> delegate,
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

  // Create (and show) the BluetoothChooser dialog.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatOriginForSecurityDisplay(origin));
  java_dialog_.Reset(std::move(create_java_dialog_callback)
                         .Run(env, window_android, origin_string,
                              delegate_->GetSecurityLevel(web_contents_),
                              delegate_->GetJavaObject(),
                              reinterpret_cast<intptr_t>(this)));
}

BluetoothChooserAndroid::BluetoothChooserAndroid(
    content::RenderFrameHost* frame,
    const EventHandler& event_handler,
    std::unique_ptr<BluetoothChooserAndroidDelegate> delegate)
    : BluetoothChooserAndroid(frame,
                              event_handler,
                              std::move(delegate),
                              GetCreateJavaBluetoothChooserDialogCallback()) {}

BluetoothChooserAndroid::~BluetoothChooserAndroid() {
  if (!java_dialog_.is_null()) {
    Java_BluetoothChooserDialog_closeDialog(AttachCurrentThread(),
                                            java_dialog_);
  }
}

bool BluetoothChooserAndroid::CanAskForScanningPermission() {
  // Creating the dialog returns null if Chromium can't ask for permission to
  // scan for BT devices.
  return !java_dialog_.is_null();
}

void BluetoothChooserAndroid::SetAdapterPresence(AdapterPresence presence) {
  JNIEnv* env = AttachCurrentThread();
  if (presence != AdapterPresence::POWERED_ON) {
    Java_BluetoothChooserDialog_notifyAdapterTurnedOff(env, java_dialog_);
  } else {
    Java_BluetoothChooserDialog_notifyAdapterTurnedOn(env, java_dialog_);
  }
}

void BluetoothChooserAndroid::ShowDiscoveryState(DiscoveryState state) {
  // These constants are used in BluetoothChooserDialog.notifyDiscoveryState.
  int java_state = -1;
  switch (state) {
    case DiscoveryState::FAILED_TO_START:
      java_state = 0;
      break;
    case DiscoveryState::DISCOVERING:
      java_state = 1;
      break;
    case DiscoveryState::IDLE:
      java_state = 2;
      break;
  }
  Java_BluetoothChooserDialog_notifyDiscoveryState(AttachCurrentThread(),
                                                   java_dialog_, java_state);
}

void BluetoothChooserAndroid::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      base::android::ConvertUTF8ToJavaString(env, device_id);
  ScopedJavaLocalRef<jstring> java_device_name =
      base::android::ConvertUTF16ToJavaString(env, device_name);
  Java_BluetoothChooserDialog_addOrUpdateDevice(
      env, java_dialog_, java_device_id, java_device_name, is_gatt_connected,
      signal_strength_level);
}

void BluetoothChooserAndroid::OnDialogFinished(
    JNIEnv* env,
    jint event_type,
    const JavaParamRef<jstring>& device_id) {
  // Values are defined in BluetoothChooserDialog as DIALOG_FINISHED constants.
  switch (event_type) {
    case 0:
      event_handler_.Run(content::BluetoothChooserEvent::DENIED_PERMISSION, "");
      return;
    case 1:
      event_handler_.Run(content::BluetoothChooserEvent::CANCELLED, "");
      return;
    case 2:
      event_handler_.Run(
          content::BluetoothChooserEvent::SELECTED,
          base::android::ConvertJavaStringToUTF8(env, device_id));
      return;
  }
  NOTREACHED_IN_MIGRATION();
}

void BluetoothChooserAndroid::RestartSearch() {
  event_handler_.Run(content::BluetoothChooserEvent::RESCAN, "");
}

void BluetoothChooserAndroid::RestartSearch(JNIEnv*) {
  RestartSearch();
}

void BluetoothChooserAndroid::ShowBluetoothOverviewLink(JNIEnv* env) {
  OpenURL(kChooserBluetoothOverviewURL);
  event_handler_.Run(content::BluetoothChooserEvent::SHOW_OVERVIEW_HELP, "");
}

void BluetoothChooserAndroid::ShowBluetoothAdapterOffLink(JNIEnv* env) {
  OpenURL(kChooserBluetoothOverviewURL);
  event_handler_.Run(content::BluetoothChooserEvent::SHOW_ADAPTER_OFF_HELP, "");
}

void BluetoothChooserAndroid::ShowNeedLocationPermissionLink(JNIEnv* env) {
  OpenURL(kChooserBluetoothOverviewURL);
  event_handler_.Run(content::BluetoothChooserEvent::SHOW_NEED_LOCATION_HELP,
                     "");
}

void BluetoothChooserAndroid::OpenURL(const char* url) {
  web_contents_->OpenURL(
      content::OpenURLParams(GURL(url), content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_AUTO_TOPLEVEL,
                             false /* is_renderer_initiated */),
      /*navigation_handle_callback=*/{});
}

}  // namespace permissions
