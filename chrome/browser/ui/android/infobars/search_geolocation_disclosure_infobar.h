// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "components/infobars/android/infobar_android.h"

class SearchGeolocationDisclosureInfoBarDelegate;

class SearchGeolocationDisclosureInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit SearchGeolocationDisclosureInfoBar(
      std::unique_ptr<SearchGeolocationDisclosureInfoBarDelegate> delegate);
  ~SearchGeolocationDisclosureInfoBar() override;

 private:
  // infobars::InfoBarAndroid:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  void ProcessButton(int action) override;

  SearchGeolocationDisclosureInfoBarDelegate* GetDelegate();

  DISALLOW_COPY_AND_ASSIGN(SearchGeolocationDisclosureInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_SEARCH_GEOLOCATION_DISCLOSURE_INFOBAR_H_
