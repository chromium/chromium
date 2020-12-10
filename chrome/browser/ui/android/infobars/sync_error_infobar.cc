// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/sync_error_infobar.h"

SyncErrorInfoBar::SyncErrorInfoBar(
    std::unique_ptr<SyncErrorInfoBarDelegateAndroid> delegate)
    : ChromeConfirmInfoBar(std::move(delegate)) {}

SyncErrorInfoBar::~SyncErrorInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
SyncErrorInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  return static_cast<SyncErrorInfoBarDelegateAndroid*>(delegate())
      ->CreateRenderInfoBar(env);
}
