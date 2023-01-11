// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INFOBARS_ANDROID_INFOBAR_ANDROID_H_
#define COMPONENTS_INFOBARS_ANDROID_INFOBAR_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "components/infobars/core/infobar.h"

namespace infobars {

class InfoBarAndroid : public InfoBar {
 public:
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.infobar
  // GENERATED_JAVA_PREFIX_TO_STRIP: ACTION_
  enum ActionType {
    ACTION_NONE = 0,
    // Confirm infobar
    ACTION_OK = 1,
    ACTION_CANCEL = 2,
    // Translate infobar
    ACTION_TRANSLATE = 3,
    ACTION_TRANSLATE_SHOW_ORIGINAL = 4,
  };

  // A function that maps from Chromium IDs to Drawable IDs.
  using ResourceIdMapper = base::RepeatingCallback<int(int)>;

  explicit InfoBarAndroid(std::unique_ptr<InfoBarDelegate> delegate);

  InfoBarAndroid(const InfoBarAndroid&) = delete;
  InfoBarAndroid& operator=(const InfoBarAndroid&) = delete;

  ~InfoBarAndroid() override;

  // InfoBar:
  virtual base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) = 0;

  virtual void SetJavaInfoBar(
      const base::android::JavaRef<jobject>& java_info_bar);
  const base::android::JavaRef<jobject>& GetJavaInfoBar();
  bool HasSetJavaInfoBar() const;

  int GetInfoBarIdentifier(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  virtual void OnLinkClicked(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj) {}
  void OnButtonClicked(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint action);
  void OnCloseButtonClicked(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);

  void CloseJavaInfoBar();

  // Acquire the java infobar from a different one.  This is used to do in-place
  // replacements.
  virtual void PassJavaInfoBar(InfoBarAndroid* source) {}

 protected:
  // Derived classes must implement this method to process the corresponding
  // action.
  virtual void ProcessButton(int action) = 0;

  void CloseInfoBar();

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_info_bar_;
};

}  // namespace infobars

#endif  // COMPONENTS_INFOBARS_ANDROID_INFOBAR_ANDROID_H_
