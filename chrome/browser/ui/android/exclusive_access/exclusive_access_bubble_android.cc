// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "exclusive_access_bubble_android.h"

#include "base/android/device_info.h"
#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "chrome/android/chrome_jni_headers/ExclusiveAccessBubble_jni.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/fullscreen_control/fullscreen_features.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace {
std::optional<std::u16string> GetOriginString(const url::Origin& origin) {
  if (origin.opaque() ||
      !base::FeatureList::IsEnabled(features::kFullscreenBubbleShowOrigin)) {
    return std::nullopt;
  }

  return url_formatter::FormatOriginForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC);
}

class BridgeImpl : public ExclusiveAccessBubbleAndroid::Bridge {
 public:
  explicit BridgeImpl(const jni_zero::JavaRef<jobject>& context) {
    if (context) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      j_bubble_.Reset(Java_ExclusiveAccessBubble_create(env, context));
    }
  }

  ~BridgeImpl() override { Hide(); }

  void Show() override {
    if (j_bubble_) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      Java_ExclusiveAccessBubble_show(env, j_bubble_);
    }
  }

  void Hide() override {
    if (j_bubble_) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      Java_ExclusiveAccessBubble_hide(env, j_bubble_);
    }
  }

  void Update(const std::u16string& text) override {
    if (j_bubble_) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      Java_ExclusiveAccessBubble_update(
          env, j_bubble_, base::android::ConvertUTF16ToJavaString(env, text));
    }
  }

  bool IsVisible() const override {
    if (j_bubble_) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      return Java_ExclusiveAccessBubble_isVisible(env, j_bubble_);
    }
    return false;
  }

  bool IsKeyboardConnected() const override {
    if (j_bubble_) {
      JNIEnv* env = jni_zero::AttachCurrentThread();
      return Java_ExclusiveAccessBubble_isKeyboardConnected(env, j_bubble_);
    }
    return false;
  }

 private:
  jni_zero::ScopedJavaGlobalRef<jobject> j_bubble_;
};
}  // namespace

ExclusiveAccessBubbleAndroid::ExclusiveAccessBubbleAndroid(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback,
    const jni_zero::ScopedJavaGlobalRef<jobject>& jcontext)
    : ExclusiveAccessBubbleAndroid(params,
                                   std::move(first_hide_callback),
                                   std::make_unique<BridgeImpl>(jcontext)) {}

ExclusiveAccessBubbleAndroid::ExclusiveAccessBubbleAndroid(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback,
    std::unique_ptr<Bridge> bridge)
    : ExclusiveAccessBubble(params), bridge_(std::move(bridge)) {
  Update(params, std::move(first_hide_callback));
}

ExclusiveAccessBubbleAndroid::~ExclusiveAccessBubbleAndroid() {
  Hide();
}

void ExclusiveAccessBubbleAndroid::Hide() {
  was_shown_ = false;
  bridge_->Hide();
}

void ExclusiveAccessBubbleAndroid::Show() {
  was_shown_ = true;
  bridge_->Show();
}

void ExclusiveAccessBubbleAndroid::HideImmediately() {
  Hide();
}

void ExclusiveAccessBubbleAndroid::Update(
    const ExclusiveAccessBubbleParams& params,
    ExclusiveAccessBubbleHideCallback first_hide_callback) {
  DCHECK(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE != params.type ||
         params.has_download);
  bool already_shown = IsVisible();
  if (params_.type == params.type && params_.origin == params.origin &&
      !params.force_update && (already_shown || was_shown_)) {
    return;
  }
  was_shown_ = true;

  // notify_overridden_ and has_download are used to change the bubble text to
  // notify the user that a download has/had started, and they need to exit the
  // exclusive access mode to see it, notify_overridden_ slightly changes the
  // text of the bubble based on the following rules:
  // 1. There was a notification visible earlier, and
  // 2. Exactly one of the previous and current notifications has a download,
  //    or the previous notification was about an override itself.
  // If both the previous and current notifications have a download, but
  // neither is an override, then we don't need to show an override.
  notify_overridden_ =
      already_shown &&
      (notify_overridden_ || (params.has_download ^ params_.has_download));
  params_.has_download = params.has_download || notify_overridden_;

  // Bubble maybe be reused after timeout.
  RunHideCallbackIfNeeded(ExclusiveAccessBubbleHideReason::kInterrupted);

  first_hide_callback_ = std::move(first_hide_callback);

  params_.origin = params.origin;
  // When a request to notify about a download is made, the bubble type
  // should be preserved from the old value, and not be updated.
  if (!params.has_download) {
    params_.type = params.type;
  }
  UpdateBubbleContent(params_.type);
  ShowAndStartTimers();
}

void ExclusiveAccessBubbleAndroid::UpdateBubbleContent(
    ExclusiveAccessBubbleType bubble_type) {
  DCHECK(params_.has_download ||
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE != bubble_type);

  bool keyboard_connected = bridge_->IsKeyboardConnected();
  std::u16string text = GetBubbleText(bubble_type, keyboard_connected);

  bridge_->Update(text);
}

std::u16string ExclusiveAccessBubbleAndroid::GetBubbleText(
    ExclusiveAccessBubbleType bubble_type,
    bool keyboard_connected) const {
  if (!keyboard_connected) {
    return exclusive_access_bubble::GetInstructionTextForTypeTouchBased(
        bubble_type, GetOriginString(params_.origin), params_.has_download,
        notify_overridden_);
  }

  std::u16string accelerator;
  bool should_show_browser_acc =
      (params_.has_download &&
       bubble_type == EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE) ||
      exclusive_access_bubble::IsExclusiveAccessModeBrowserFullscreen(
          bubble_type);
  if (should_show_browser_acc &&
      !base::FeatureList::IsEnabled(
          features::kPressAndHoldEscToExitBrowserFullscreen)) {
    accelerator = browser_fullscreen_exit_accelerator_;
  } else {
    accelerator = l10n_util::GetStringUTF16(IDS_APP_ESC_KEY);
  }

  return exclusive_access_bubble::GetInstructionTextForType(
      params_.type, accelerator, GetOriginString(params_.origin),
      params_.has_download, notify_overridden_);
}

bool ExclusiveAccessBubbleAndroid::IsVisible() const {
  return bridge_->IsVisible();
}

void ExclusiveAccessBubbleAndroid::RunHideCallbackIfNeeded(
    ExclusiveAccessBubbleHideReason reason) {
  if (first_hide_callback_) {
    std::move(first_hide_callback_).Run(reason);
  }
}

DEFINE_JNI(ExclusiveAccessBubble)
