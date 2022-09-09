// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "components/infobars/android/confirm_infobar.h"

namespace android {
class DuplicateDownloadInfoBarDelegate;
}

// Records user interactions with the duplicate download infobar.
// Used in UMA, do not remove, change or reuse existing entries.
// Update histograms.xml and enums.xml when adding entries.
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.infobar
enum class DuplicateDownloadInfobarEvent {
  // Infobar was shown.
  kShown = 0,
  // Accepted the duplicate download.
  kAccepted = 1,
  // Canceled the duplicate download.
  kCanceled = 2,
  // Link on the infobar is clicked.
  kLinkClicked = 3,
  // Dismissed the duplicate download.
  kDismissed = 4,
  kCount
};

// A native-side implementation of an infobar to ask whether to continue
// downloading if target file already exists.
class DuplicateDownloadInfoBar : public infobars::ConfirmInfoBar {
 public:
  static std::unique_ptr<infobars::InfoBar> CreateInfoBar(
      std::unique_ptr<android::DuplicateDownloadInfoBarDelegate> delegate);

  DuplicateDownloadInfoBar(const DuplicateDownloadInfoBar&) = delete;
  DuplicateDownloadInfoBar& operator=(const DuplicateDownloadInfoBar&) = delete;

  ~DuplicateDownloadInfoBar() override;

  static void RecordDuplicateDownloadInfobarEvent(
      bool is_offline_page,
      DuplicateDownloadInfobarEvent event);

 private:
  explicit DuplicateDownloadInfoBar(
      std::unique_ptr<android::DuplicateDownloadInfoBarDelegate> delegate);

  // ConfirmInfoBar:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;

  android::DuplicateDownloadInfoBarDelegate* GetDelegate();
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_DUPLICATE_DOWNLOAD_INFOBAR_H_
