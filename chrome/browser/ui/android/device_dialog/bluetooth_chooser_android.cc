// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/bluetooth_chooser_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/BluetoothChooserDialog_jni.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/common/url_constants.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

BluetoothChooserAndroid::BluetoothChooserAndroid(
    content::RenderFrameHost* frame,
    const EventHandler& event_handler)
    : web_contents_(content::WebContents::FromRenderFrameHost(frame)),
      event_handler_(event_handler) {
  const url::Origin origin = frame->GetLastCommittedOrigin();
  DCHECK(!origin.opaque());

  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents_->GetNativeView()->GetWindowAndroid()->GetJavaObject();

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents_);
  DCHECK(helper);

  // Create (and show) the BluetoothChooser dialog.
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> origin_string =
      base::android::ConvertUTF16ToJavaString(
          env, url_formatter::FormatOriginForSecurityDisplay(origin));
  java_dialog_.Reset(Java_BluetoothChooserDialog_create(
      env, window_android, origin_string, helper->GetSecurityLevel(),
      reinterpret_cast<intptr_t>(this)));
}

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
    const base::string16& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> java_device_id =
      ConvertUTF8ToJavaString(env, device_id);
  ScopedJavaLocalRef<jstring> java_device_name =
      ConvertUTF16ToJavaString(env, device_name);
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
      event_handler_.Run(Event::DENIED_PERMISSION, "");
      return;
    case 1:
      event_handler_.Run(Event::CANCELLED, "");
      return;
    case 2:
      event_handler_.Run(
          Event::SELECTED,
          base::android::ConvertJavaStringToUTF8(env, device_id));
      return;
  }
  NOTREACHED();
}

void BluetoothChooserAndroid::RestartSearch() {
  event_handler_.Run(Event::RESCAN, "");
}

void BluetoothChooserAndroid::RestartSearch(JNIEnv*) {
  RestartSearch();
}

void BluetoothChooserAndroid::ShowBluetoothOverviewLink(JNIEnv* env) {
  OpenURL(chrome::kChooserBluetoothOverviewURL);
  event_handler_.Run(Event::SHOW_OVERVIEW_HELP, "");
}

void BluetoothChooserAndroid::ShowBluetoothAdapterOffLink(JNIEnv* env) {
  OpenURL(chrome::kChooserBluetoothOverviewURL);
  event_handler_.Run(Event::SHOW_ADAPTER_OFF_HELP, "");
}

void BluetoothChooserAndroid::ShowNeedLocationPermissionLink(JNIEnv* env) {
  OpenURL(chrome::kChooserBluetoothOverviewURL);
  event_handler_.Run(Event::SHOW_NEED_LOCATION_HELP, "");
}

void BluetoothChooserAndroid::OpenURL(const char* url) {
  web_contents_->OpenURL(content::OpenURLParams(
      GURL(url), content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui::PAGE_TRANSITION_AUTO_TOPLEVEL, false /* is_renderer_initiated */));
}
