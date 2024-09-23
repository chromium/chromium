// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/messages/android/jni_headers/SimpleConfirmInfoBarBuilder_jni.h"

using base::android::JavaParamRef;

namespace {

// Delegate for a simple ConfirmInfoBar triggered via JNI.
class SimpleConfirmInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  SimpleConfirmInfoBarDelegate(
      const JavaParamRef<jobject>& j_listener,
      infobars::InfoBarDelegate::InfoBarIdentifier infobar_identifier,
      const gfx::Image& bitmap,
      const std::u16string& message_str,
      const std::u16string& primary_str,
      const std::u16string& secondary_str,
      const std::u16string& link_text_str,
      bool auto_expire);

  SimpleConfirmInfoBarDelegate(const SimpleConfirmInfoBarDelegate&) = delete;
  SimpleConfirmInfoBarDelegate& operator=(const SimpleConfirmInfoBarDelegate&) =
      delete;

  ~SimpleConfirmInfoBarDelegate() override;

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  ui::ImageModel GetIcon() const override;
  std::u16string GetLinkText() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool LinkClicked(WindowOpenDisposition disposition) override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_listener_;
  infobars::InfoBarDelegate::InfoBarIdentifier identifier_;
  gfx::Image icon_bitmap_;
  std::u16string message_str_;
  std::u16string primary_str_;
  std::u16string secondary_str_;
  std::u16string link_text_str_;
  bool auto_expire_;
};

SimpleConfirmInfoBarDelegate::SimpleConfirmInfoBarDelegate(
    const JavaParamRef<jobject>& j_listener,
    infobars::InfoBarDelegate::InfoBarIdentifier identifier,
    const gfx::Image& bitmap,
    const std::u16string& message_str,
    const std::u16string& primary_str,
    const std::u16string& secondary_str,
    const std::u16string& link_text_str,
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

ui::ImageModel SimpleConfirmInfoBarDelegate::GetIcon() const {
  return icon_bitmap_.IsEmpty() ? ConfirmInfoBarDelegate::GetIcon()
                                : ui::ImageModel::FromImage(icon_bitmap_);
}

std::u16string SimpleConfirmInfoBarDelegate::GetLinkText() const {
  return link_text_str_;
}

bool SimpleConfirmInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return auto_expire_ && ConfirmInfoBarDelegate::ShouldExpire(details);
}

bool SimpleConfirmInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  return !Java_SimpleConfirmInfoBarBuilder_onInfoBarLinkClicked(
      base::android::AttachCurrentThread(), java_listener_);
}

void SimpleConfirmInfoBarDelegate::InfoBarDismissed() {
  Java_SimpleConfirmInfoBarBuilder_onInfoBarDismissed(
      base::android::AttachCurrentThread(), java_listener_);
}

std::u16string SimpleConfirmInfoBarDelegate::GetMessageText() const {
  return message_str_;
}

int SimpleConfirmInfoBarDelegate::GetButtons() const {
  return (primary_str_.empty() ? 0 : BUTTON_OK) |
      (secondary_str_.empty() ? 0 : BUTTON_CANCEL);
}

std::u16string SimpleConfirmInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
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

}  // anonymous namespace

// Native JNI methods ---------------------------------------------------------

void JNI_SimpleConfirmInfoBarBuilder_Create(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_web_contents,
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

  std::u16string message_str =
      j_message.is_null()
          ? std::u16string()
          : base::android::ConvertJavaStringToUTF16(env, j_message);
  std::u16string primary_str =
      j_primary.is_null()
          ? std::u16string()
          : base::android::ConvertJavaStringToUTF16(env, j_primary);
  std::u16string secondary_str =
      j_secondary.is_null()
          ? std::u16string()
          : base::android::ConvertJavaStringToUTF16(env, j_secondary);
  std::u16string link_text_str =
      j_link_text.is_null()
          ? std::u16string()
          : base::android::ConvertJavaStringToUTF16(env, j_link_text);

  infobars::ContentInfoBarManager* manager =
      infobars::ContentInfoBarManager::FromWebContents(
          content::WebContents::FromJavaWebContents(j_web_contents));
  manager->AddInfoBar(std::make_unique<infobars::ConfirmInfoBar>(
      std::make_unique<SimpleConfirmInfoBarDelegate>(
          j_listener, infobar_identifier, icon_bitmap, message_str, primary_str,
          secondary_str, link_text_str, auto_expire)));
}
