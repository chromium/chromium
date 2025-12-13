// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/serial_chooser_dialog_android.h"

#include <jni.h>
#include <stddef.h>

#include <string>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "components/permissions/chooser_controller.h"
#include "components/permissions/permission_util.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/SerialChooserDialog_jni.h"

namespace {

std::unique_ptr<SerialChooserDialogAndroid::JavaDialog>
CreateJavaSerialChooserDialog(
    JNIEnv* env,
    const base::android::JavaRef<jobject>&
        window_android,  // Java Type: WindowAndroid
    const std::u16string& origin,
    security_state::SecurityLevel security_level,
    const base::android::JavaRef<jobject>& profile,  // Java Type: Profile
    SerialChooserDialogAndroid* dialog) {
  base::android::ScopedJavaLocalRef<jobject> j_dialog =
      Java_SerialChooserDialog_create(env, window_android, origin,
                                      security_level, profile,
                                      reinterpret_cast<intptr_t>(dialog));
  if (j_dialog.is_null()) {
    return nullptr;
  }
  return std::make_unique<SerialChooserDialogAndroid::JavaDialog>(j_dialog);
}

SerialChooserDialogAndroid::CreateJavaDialogCallback
GetCreateJavaSerialChooserDialogCallback() {
  return base::BindOnce(&CreateJavaSerialChooserDialog);
}

}  // namespace

// JavaDialog

SerialChooserDialogAndroid::JavaDialog::JavaDialog(
    const base::android::JavaRef<jobject>& dialog)
    : j_dialog_(dialog) {}

SerialChooserDialogAndroid::JavaDialog::~JavaDialog() {
  Close();
}

void SerialChooserDialogAndroid::JavaDialog::Close() {
  if (j_dialog_.is_null()) {
    return;
  }
  Java_SerialChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                       j_dialog_);
  j_dialog_.Reset();
}

void SerialChooserDialogAndroid::JavaDialog::SetIdleState() const {
  Java_SerialChooserDialog_setIdleState(base::android::AttachCurrentThread(),
                                        j_dialog_);
}

void SerialChooserDialogAndroid::JavaDialog::AddDevice(
    const std::string& item_id,
    const std::u16string& device_name) const {
  Java_SerialChooserDialog_addDevice(base::android::AttachCurrentThread(),
                                     j_dialog_, item_id, device_name);
}

void SerialChooserDialogAndroid::JavaDialog::RemoveDevice(
    const std::string& item_id) const {
  Java_SerialChooserDialog_removeDevice(base::android::AttachCurrentThread(),
                                        j_dialog_, item_id);
}

void SerialChooserDialogAndroid::JavaDialog::OnAdapterEnabledChanged(
    bool enabled) const {
  Java_SerialChooserDialog_onAdapterEnabledChanged(
      base::android::AttachCurrentThread(), j_dialog_, enabled);
}

void SerialChooserDialogAndroid::JavaDialog::OnAdapterAuthorizationChanged(
    bool authorized) const {
  Java_SerialChooserDialog_onAdapterAuthorizationChanged(
      base::android::AttachCurrentThread(), j_dialog_, authorized);
}

// SerialChooserDialogAndroid

// static
std::unique_ptr<SerialChooserDialogAndroid> SerialChooserDialogAndroid::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller,
    base::OnceClosure on_close) {
  return CreateInternal(render_frame_host, std::move(controller),
                        std::move(on_close),
                        GetCreateJavaSerialChooserDialogCallback());
}

// static
std::unique_ptr<SerialChooserDialogAndroid>
SerialChooserDialogAndroid::CreateForTesting(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller,
    base::OnceClosure on_close,
    CreateJavaDialogCallback create_java_dialog_callback) {
  return CreateInternal(render_frame_host, std::move(controller),
                        std::move(on_close),
                        std::move(create_java_dialog_callback));
}

// static
std::unique_ptr<SerialChooserDialogAndroid>
SerialChooserDialogAndroid::CreateInternal(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller,
    base::OnceClosure on_close,
    CreateJavaDialogCallback create_java_dialog_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);

  // Create (and show) the SerialChooser dialog.
  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject();
  JNIEnv* env = base::android::AttachCurrentThread();
  // Permission delegation means the permission request should be
  // attributed to the main frame.
  const auto origin = url::Origin::Create(
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()));
  std::u16string origin_string =
      url_formatter::FormatOriginForSecurityDisplay(origin);
  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  CHECK(helper);

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  CHECK(profile);

  base::android::ScopedJavaLocalRef<jobject> j_profile_android =
      profile->GetJavaObject();
  CHECK(!j_profile_android.is_null());

  auto dialog = std::make_unique<SerialChooserDialogAndroid>(
      std::move(controller), std::move(on_close));

  dialog->java_dialog_ =
      std::move(create_java_dialog_callback)
          .Run(env, window_android, origin_string, helper->GetSecurityLevel(),
               j_profile_android, dialog.get());
  if (!dialog->java_dialog_) {
    return nullptr;
  }

  return dialog;
}

SerialChooserDialogAndroid::SerialChooserDialogAndroid(
    std::unique_ptr<permissions::ChooserController> controller,
    base::OnceClosure on_close)
    : controller_(std::move(controller)), on_close_(std::move(on_close)) {
  controller_->set_view(this);
}

SerialChooserDialogAndroid::~SerialChooserDialogAndroid() {
  controller_->set_view(nullptr);
}

void SerialChooserDialogAndroid::ListDevices(JNIEnv* env) {
  controller_->RefreshOptions();
}

void SerialChooserDialogAndroid::OnOptionsInitialized() {
  while (!item_id_map_.empty()) {
    OnOptionRemoved(item_id_map_.size() - 1);
  }

  for (size_t i = 0; i < controller_->NumOptions(); ++i) {
    OnOptionAdded(i);
  }

  java_dialog_->SetIdleState();
}

void SerialChooserDialogAndroid::OnOptionAdded(size_t index) {
  CHECK_LE(index, item_id_map_.size());
  int item_id = next_item_id_++;
  std::string item_id_str = base::NumberToString(item_id);
  item_id_map_.insert(item_id_map_.begin() + index, item_id_str);

  std::u16string device_name = controller_->GetOption(index);
  java_dialog_->AddDevice(item_id_str, device_name);
}

void SerialChooserDialogAndroid::OnOptionRemoved(size_t index) {
  CHECK_LT(index, item_id_map_.size());
  std::string item_id = item_id_map_[index];
  item_id_map_.erase(item_id_map_.begin() + index);

  java_dialog_->RemoveDevice(item_id);
}

void SerialChooserDialogAndroid::OnOptionUpdated(size_t index) {
  NOTREACHED();
}

void SerialChooserDialogAndroid::OnAdapterEnabledChanged(bool enabled) {
  java_dialog_->OnAdapterEnabledChanged(enabled);
}

void SerialChooserDialogAndroid::OnRefreshStateChanged(bool refreshing) {
  NOTREACHED();
}

void SerialChooserDialogAndroid::OnAdapterAuthorizationChanged(
    bool authorized) {
  java_dialog_->OnAdapterAuthorizationChanged(authorized);
}

void SerialChooserDialogAndroid::OnItemSelected(JNIEnv* env,
                                                std::string& item_id) {
  auto it = std::ranges::find(item_id_map_, item_id);
  CHECK(it != item_id_map_.end());
  controller_->Select(
      {static_cast<size_t>(std::distance(item_id_map_.begin(), it))});
  std::move(on_close_).Run();
}

void SerialChooserDialogAndroid::OnDialogCancelled(JNIEnv* env) {
  Cancel();
}

void SerialChooserDialogAndroid::OpenSerialHelpPage(JNIEnv* env) {
  controller_->OpenHelpCenterUrl();
  Cancel();
}

void SerialChooserDialogAndroid::OpenAdapterOffHelpPage(JNIEnv* env) {
  controller_->OpenAdapterOffHelpUrl();
  Cancel();
}

void SerialChooserDialogAndroid::OpenBluetoothPermissionHelpPage(JNIEnv* env) {
  controller_->OpenBluetoothPermissionHelpUrl();
  Cancel();
}

void SerialChooserDialogAndroid::Cancel() {
  controller_->Cancel();
  std::move(on_close_).Run();
}

DEFINE_JNI(SerialChooserDialog)
