// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_

#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-shared.h"

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using TokenError = content::IdentityCredentialTokenError;

// This class provides an implementation of the AccountSelectionView interface
// and communicates via JNI with its AccountSelectionBridge Java counterpart.
class AccountSelectionViewAndroid : public AccountSelectionView {
 public:
  explicit AccountSelectionViewAndroid(
      AccountSelectionView::Delegate* delegate);
  ~AccountSelectionViewAndroid() override;

  // AccountSelectionView:
  bool Show(
      const std::string& rp_for_display,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      Account::SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode,
      const std::vector<IdentityRequestAccountPtr>& new_accounts) override;
  bool ShowFailureDialog(
      const std::string& rp_for_display,
      const std::string& idp_for_display,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      const content::IdentityProviderMetadata& idp_metadata) override;
  bool ShowErrorDialog(const std::string& rp_for_display,
                       const std::string& idp_for_display,
                       blink::mojom::RpContext rp_context,
                       blink::mojom::RpMode rp_mode,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override;
  bool ShowLoadingDialog(const std::string& rp_for_display,
                         const std::string& idp_for_display,
                         blink::mojom::RpContext rp_context,
                         blink::mojom::RpMode rp_mode) override;

  std::string GetTitle() const override;
  std::optional<std::string> GetSubtitle() const override;
  void ShowUrl(LinkType link_type, const GURL& url) override;
  content::WebContents* ShowModalDialog(const GURL& url,
                                        blink::mojom::RpMode rp_mode) override;
  void CloseModalDialog() override;
  content::WebContents* GetRpWebContents() override;

  void OnAccountSelected(JNIEnv* env,
                         const GURL& idp_config_url,
                         const std::vector<std::string>& account_string_fields,
                         const GURL& account_picture_url,
                         bool is_sign_in);
  void OnDismiss(JNIEnv* env, jint dismiss_reason);
  void OnLoginToIdP(JNIEnv* env,
                    const GURL& idp_config_url,
                    const GURL& idp_login_url);
  void OnMoreDetails(JNIEnv* env);
  void OnAccountsDisplayed(JNIEnv* env);

 private:
  // Returns either true if the java counterpart of this bridge is initialized
  // successfully or false if the creation failed.
  bool MaybeCreateJavaObject(std::optional<blink::mojom::RpMode> rp_mode);

  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;
};

#endif  // CHROME_BROWSER_UI_ANDROID_WEBID_ACCOUNT_SELECTION_VIEW_ANDROID_H_
