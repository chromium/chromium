// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webid/account_selection_view.h"

// This class provides an implementation of the AccountSelectionView interface
// and communicates via JNI with its AccountSelectionBridge Java counterpart.
class AccountSelectionViewAndroid : public AccountSelectionView {
 public:
  explicit AccountSelectionViewAndroid(
      AccountSelectionView::Delegate* delegate);
  ~AccountSelectionViewAndroid() override;

  // AccountSelectionView:
  void Show(
      const std::string& top_frame_for_display,
      const absl::optional<std::string>& iframe_url_for_display,
      const std::vector<content::IdentityProviderData>& identity_provider_data,
      Account::SignInMode sign_in_mode,
      bool show_auto_reauthn_checkbox) override;
  void ShowFailureDialog(
      const std::string& top_frame_for_display,
      const std::string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) override;
  std::string GetTitle() const override;
  absl::optional<std::string> GetSubtitle() const override;

  void OnAccountSelected(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& idp_config_url,
      const base::android::JavaParamRef<jobjectArray>& account_string_fields,
      const base::android::JavaParamRef<jobject>& account_picture_url,
      bool is_sign_in);
  void OnDismiss(JNIEnv* env, jint dismiss_reason);

 private:
  // Returns either true if the java counterpart of this bridge is initialized
  // successfully or false if the creation failed. This method will recreate the
  // java object whenever Show() is called.
  bool RecreateJavaObject();

  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_
