// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/duplicate_download_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/download/android/duplicate_download_infobar_delegate.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/DuplicateDownloadInfoBar_jni.h"

using android::DuplicateDownloadInfoBarDelegate;

// static
std::unique_ptr<infobars::InfoBar> DuplicateDownloadInfoBar::CreateInfoBar(
    std::unique_ptr<DuplicateDownloadInfoBarDelegate> delegate) {
  return base::WrapUnique(new DuplicateDownloadInfoBar(std::move(delegate)));
}

DuplicateDownloadInfoBar::~DuplicateDownloadInfoBar() {
}

DuplicateDownloadInfoBar::DuplicateDownloadInfoBar(
    std::unique_ptr<DuplicateDownloadInfoBarDelegate> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

base::android::ScopedJavaLocalRef<jobject>
DuplicateDownloadInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  DuplicateDownloadInfoBarDelegate* delegate = GetDelegate();

  base::android::ScopedJavaLocalRef<jobject> j_otr_profile_id;
  if (delegate->GetOTRProfileID()) {
    j_otr_profile_id =
        delegate->GetOTRProfileID()->ConvertToJavaOTRProfileID(env);
  }
  base::android::ScopedJavaLocalRef<jobject> java_infobar(
      Java_DuplicateDownloadInfoBar_createInfoBar(
          env, delegate->GetFilePath(), delegate->IsOfflinePage(),
          delegate->GetPageURL(), j_otr_profile_id,
          delegate->DuplicateRequestExists()));
  return java_infobar;
}

DuplicateDownloadInfoBarDelegate* DuplicateDownloadInfoBar::GetDelegate() {
  return static_cast<DuplicateDownloadInfoBarDelegate*>(delegate());
}
