// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/search_geolocation_disclosure_infobar.h"

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/SearchGeolocationDisclosureInfoBar_jni.h"
#include "chrome/browser/android/search_permissions/search_geolocation_disclosure_infobar_delegate.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

SearchGeolocationDisclosureInfoBar::SearchGeolocationDisclosureInfoBar(
    std::unique_ptr<SearchGeolocationDisclosureInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

SearchGeolocationDisclosureInfoBar::~SearchGeolocationDisclosureInfoBar() {
}

ScopedJavaLocalRef<jobject>
SearchGeolocationDisclosureInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetDelegate()->message_text());
  return Java_SearchGeolocationDisclosureInfoBar_show(
      env, GetEnumeratedIconId(), message_text,
      GetDelegate()->inline_link_range().start(),
      GetDelegate()->inline_link_range().end());
}

void SearchGeolocationDisclosureInfoBar::OnLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  GetDelegate()->RecordSettingsClicked();

  ScopedJavaLocalRef<jstring> search_url =
      base::android::ConvertUTF8ToJavaString(
          env, GetDelegate()->search_url().spec());
  Java_SearchGeolocationDisclosureInfoBar_showSettingsPage(env, search_url);
  RemoveSelf();
}

void SearchGeolocationDisclosureInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  RemoveSelf();
}

SearchGeolocationDisclosureInfoBarDelegate*
SearchGeolocationDisclosureInfoBar::GetDelegate() {
  return static_cast<SearchGeolocationDisclosureInfoBarDelegate*>(delegate());
}
