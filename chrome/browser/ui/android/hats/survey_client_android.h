// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CLIENT_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CLIENT_ANDROID_H_

#include <jni.h>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/hats/survey_ui_delegate_android.h"
#include "chrome/browser/ui/hats/survey_config.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace ui {
class WindowAndroid;
}  // namespace ui

namespace hats {

// Key-value mapping type for survey's product specific bits data.
typedef std::map<std::string, bool> SurveyBitsData;

// Key-value mapping type for survey's product specific string data.
typedef std::map<std::string, std::string> SurveyStringData;

// C++ equivalent of Java SurveyClient.
class SurveyClientAndroid {
 public:
  explicit SurveyClientAndroid(
      const std::string& trigger,
      SurveyUiDelegateAndroid* ui_delegate,
      Profile* profile,
      const std::optional<std::string>& supplied_trigger_id);

  SurveyClientAndroid(const SurveyClientAndroid&) = delete;
  SurveyClientAndroid& operator=(const SurveyClientAndroid&) = delete;

  ~SurveyClientAndroid();

  // Launch the survey with identifier if appropriate. Similar interface to
  // |HatsService::LaunchSurvey|.
  void LaunchSurvey(ui::WindowAndroid* window,
                    const SurveyBitsData& product_specific_bits_data = {},
                    const SurveyStringData& product_specific_string_data = {});

  // Destroy the instance and clean up the dependencies.
  void Destroy();

 private:
  std::unique_ptr<SurveyUiDelegateAndroid> ui_delegate_;
  base::android::ScopedJavaGlobalRef<jobject> jobj_;
};

}  // namespace hats

#endif  // CHROME_BROWSER_UI_ANDROID_HATS_SURVEY_CLIENT_ANDROID_H_
