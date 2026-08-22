// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/device_dialog/hid_chooser_dialog_android.h"

#include <algorithm>
#include <cstddef>
#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "components/permissions/permission_util.h"
#include "components/security_state/core/security_state.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/HidChooserDialog_jni.h"

namespace {

HidChooserDialogAndroid::CreateJavaDialogCallback
GetCreateJavaHidChooserDialogCallback() {
  return base::BindOnce(&Java_HidChooserDialog_create);
}

}  // namespace

// static
base::OnceClosure HidChooserDialogAndroid::Create(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller) {
  return CreateInternal(render_frame_host, std::move(controller),
                        GetCreateJavaHidChooserDialogCallback());
}

// static
base::OnceClosure HidChooserDialogAndroid::CreateForTesting(  // IN-TEST
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller,
    CreateJavaDialogCallback create_java_dialog_callback) {
  return CreateInternal(render_frame_host, std::move(controller),
                        std::move(create_java_dialog_callback));
}

// static
base::OnceClosure HidChooserDialogAndroid::CreateInternal(
    content::RenderFrameHost* render_frame_host,
    std::unique_ptr<permissions::ChooserController> controller,
    CreateJavaDialogCallback create_java_dialog_callback) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  if (!web_contents || !web_contents->GetNativeView() ||
      !web_contents->GetNativeView()->GetWindowAndroid()) {
    return base::OnceClosure();
  }

  base::android::ScopedJavaLocalRef<jobject> window_android =
      web_contents->GetNativeView()->GetWindowAndroid()->GetJavaObject();
  if (window_android.is_null()) {
    return base::OnceClosure();
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  const auto origin = url::Origin::Create(
      permissions::PermissionUtil::GetLastCommittedOriginAsURL(
          render_frame_host->GetMainFrame()));
  std::u16string origin_string =
      url_formatter::FormatOriginForSecurityDisplay(origin);
  int security_level = chrome_security_state::GetSecurityLevel(web_contents);

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  CHECK(profile);

  base::android::ScopedJavaLocalRef<jobject> j_profile_android =
      profile->GetJavaObject();
  CHECK(!j_profile_android.is_null());

  auto dialog =
      std::make_unique<HidChooserDialogAndroid>(std::move(controller));

  dialog->java_dialog_.Reset(
      std::move(create_java_dialog_callback)
          .Run(env, window_android, origin_string, security_level,
               j_profile_android, reinterpret_cast<intptr_t>(dialog.get())));
  if (dialog->java_dialog_.is_null()) {
    return base::OnceClosure();
  }

  return base::BindOnce(
      [](std::unique_ptr<HidChooserDialogAndroid> dialog) {
        base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
            FROM_HERE, std::move(dialog));
      },
      std::move(dialog));
}

HidChooserDialogAndroid::HidChooserDialogAndroid(
    std::unique_ptr<permissions::ChooserController> controller)
    : controller_(std::move(controller)) {
  controller_->set_view(this);
}

HidChooserDialogAndroid::~HidChooserDialogAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!java_dialog_.is_null()) {
    Java_HidChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                      java_dialog_);
  }
  controller_->set_view(nullptr);
}

void HidChooserDialogAndroid::OnOptionsInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (size_t i = 0; i < controller_->NumOptions(); ++i) {
    OnOptionAdded(i);
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_HidChooserDialog_setIdleState(env, java_dialog_);
}

void HidChooserDialogAndroid::OnOptionAdded(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();

  CHECK_LE(index, item_id_map_.size());
  int item_id = next_item_id_++;
  std::string item_id_str = base::NumberToString(item_id);
  item_id_map_.insert(item_id_map_.begin() + index, item_id_str);

  std::u16string device_name = controller_->GetOption(index);
  Java_HidChooserDialog_addDevice(env, java_dialog_, item_id_str, device_name);
}

void HidChooserDialogAndroid::OnOptionRemoved(size_t index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();

  CHECK_LT(index, item_id_map_.size());
  std::string item_id = item_id_map_[index];
  item_id_map_.erase(item_id_map_.begin() + index);

  Java_HidChooserDialog_removeDevice(env, java_dialog_, item_id);
}

void HidChooserDialogAndroid::OnOptionUpdated(size_t index) {
  NOTREACHED();
}

void HidChooserDialogAndroid::OnAdapterEnabledChanged(bool enabled) {
  NOTREACHED();
}

void HidChooserDialogAndroid::OnRefreshStateChanged(bool refreshing) {
  NOTREACHED();
}

void HidChooserDialogAndroid::OnItemSelected(JNIEnv* env,
                                             const std::string& item_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (item_selected_) {
    return;
  }
  item_selected_ = true;
  auto it = std::ranges::find(item_id_map_, item_id);
  CHECK(it != item_id_map_.end());
  controller_->Select(
      {static_cast<size_t>(std::distance(item_id_map_.begin(), it))});
}

void HidChooserDialogAndroid::OnDialogCancelled(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Cancel();
}

void HidChooserDialogAndroid::LoadHidHelpPage(JNIEnv* env) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (item_selected_) {
    return;
  }
  controller_->OpenHelpCenterUrl();
  Cancel();
}

void HidChooserDialogAndroid::Cancel() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (item_selected_) {
    return;
  }
  item_selected_ = true;
  if (!java_dialog_.is_null()) {
    Java_HidChooserDialog_closeDialog(base::android::AttachCurrentThread(),
                                      java_dialog_);
  }
  controller_->Cancel();
}

DEFINE_JNI(HidChooserDialog)
