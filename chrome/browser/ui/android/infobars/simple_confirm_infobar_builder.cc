// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/android/chrome_jni_headers/SimpleConfirmInfoBarBuilder_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::JavaParamRef;

namespace {

// Delegate for a simple ConfirmInfoBar triggered via JNI.
class SimpleConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SimpleConfirmInfoBarDelegate(
      const JavaParamRef<jobject>& j_listener,
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      const gfx::Image& bitmap,
      const base::string16& message_str,
      const base::string16& primary_str,
      const base::string16& secondary_str,
      const base::string16& link_text_str,
      bool auto_expire);

  ~SimpleConfirmInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  gfx::Image GetIcon() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  base::string16 GetMessageText() const override;
  int GetButtons() const override;
  base::string16 GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;
  base::string16 GetLinkText() const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_listener_;
  infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
  gfx::Image icon_bitmap_;
  base::string16 message_str_;
  base::string16 primary_str_;
  base::string16 secondary_str_;
  base::string16 link_text_str_;
  bool auto_expire_;

  DISALLOW_COPY_AND_ASSIGN(SimpleConfirmInfoBarDelegate);
};

SimpleConfirmInfoBarDelegate::SimpleConfirmInfoBarDelegate(
    const JavaParamRef<jobject>& j_listener,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    const gfx::Image& bitmap,
    const base::string16& message_str,
    const base::string16& primary_str,
    const base::string16& secondary_str,
    const base::string16& link_text_str,
    bool auto_expire)
    : identifier_(identifier),
      icon_bitmap_(bitmap),
      message_str_(message_str),
      primary_str_(primary_str),
      secondary_str_(secondary_str),
      link_text_str_(link_text_str),
      auto_expire_(auto_expire) {
  java_listener_.Reset(j_listener);
}

SimpleConfirmInfoBarDelegate::~SimpleConfirmInfoBarDelegate() {
}

infobars::InfoBarDelegate::InfoBarIdentifier
SimpleConfirmInfoBarDelegate::GetIdentifier() const {
  return identifier_;
}

gfx::Image SimpleConfirmInfoBarDelegate::GetIcon() const {
  return icon_bitmap_.IsEmpty() ? ConfirmInfoBarDelegate::GetIcon()
                                : icon_bitmap_;
}

bool SimpleConfirmInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return auto_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

void SimpleConfirmInfoBarDelegate::InfoBarDismissed() {
  Java_SimpleConfirmInfoBarBuilder_onInfoBarDismissed(
      base::android::AttachCurrentThread(), java_listener_);
}

base::string16 SimpleConfirmInfoBarDelegate::GetMessageText() const {
  return message_str_;
}

int SimpleConfirmInfoBarDelegate::GetButtons() const {
  return (primary_str_.empty() ? 0 : BUTTON_OK) |
      (secondary_str_.empty() ? 0 : BUTTON_CANCEL);
}

base::string16
SimpleConfirmInfoBarDelegate::GetButtonLabel(InfoBarButton button) const {
  return button == BUTTON_OK ? primary_str_ : secondary_str_;
}

bool SimpleConfirmInfoBarDelegate::Accept() {
  return !Java_SimpleConfirmInfoBarBuilder_onInfoBarButtonClicked(
      base::android::AttachCurrentThread(), java_listener_, true);
}

bool SimpleConfirmInfoBarDelegate::Cancel() {
  return !Java_SimpleConfirmInfoBarBuilder_onInfoBarButtonClicked(
      base::android::AttachCurrentThread(), java_listener_, false);
}

base::string16 SimpleConfirmInfoBarDelegate::GetLinkText() const {
  return link_text_str_;
}

bool SimpleConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  return !Java_SimpleConfirmInfoBarBuilder_onInfoBarLinkClicked(
      base::android::AttachCurrentThread(), java_listener_);
}

}  // anonymous namespace

// Native JNI methods ---------------------------------------------------------

void JNI_SimpleConfirmInfoBarBuilder_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_tab,
    jint j_identifier,
    const JavaParamRef<jobject>& j_icon,
    const JavaParamRef<jstring>& j_message,
    const JavaParamRef<jstring>& j_primary,
    const JavaParamRef<jstring>& j_secondary,
    const JavaParamRef<jstring>& j_link_text,
    jboolean auto_expire,
    const JavaParamRef<jobject>& j_listener) {
  infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier =
      static_cast<infobars::InfoBarDelegate::InfoBarIdentifier>(j_identifier);

  gfx::Image icon_bitmap;
  if (j_icon) {
    icon_bitmap = gfx::Image::CreateFrom1xBitmap(
        gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(j_icon)));
  }

  base::string16 message_str = j_message.is_null()
      ? base::string16()
      : base::android::ConvertJavaStringToUTF16(env, j_message);
  base::string16 primary_str = j_primary.is_null()
      ? base::string16()
      : base::android::ConvertJavaStringToUTF16(env, j_primary);
  base::string16 secondary_str = j_secondary.is_null()
      ? base::string16()
      : base::android::ConvertJavaStringToUTF16(env, j_secondary);
  base::string16 link_text_str =
      j_link_text.is_null()
          ? base::string16()
          : base::android::ConvertJavaStringToUTF16(env, j_link_text);

  InfoBarService* service = InfoBarService::FromWebContents(
      TabAndroid::GetNativeTab(env, j_tab)->web_contents());
  service->AddInfoBar(service->CreateConfirmInfoBar(
      std::make_unique<SimpleConfirmInfoBarDelegate>(
          j_listener, infobar_identifier, icon_bitmap, message_str, primary_str,
          secondary_str, link_text_str, auto_expire)));
}
