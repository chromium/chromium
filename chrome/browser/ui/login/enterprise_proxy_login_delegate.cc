// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/login/enterprise_proxy_login_delegate.h"

#include "base/check.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/android/signin_bridge.h"
#include "chrome/browser/signin/android/signin_bridge_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#endif

EnterpriseProxyLoginDelegate::EnterpriseProxyLoginDelegate(
    base::WeakPtr<content::WebContents> web_contents,
    bool is_primary_main_frame)
    : web_contents_(web_contents),
      is_primary_main_frame_(is_primary_main_frame) {}

EnterpriseProxyLoginDelegate::~EnterpriseProxyLoginDelegate() = default;

void EnterpriseProxyLoginDelegate::OnSignInRequired(
    const GURL& destination_url) {
#if BUILDFLAG(IS_ANDROID)
  if (!is_primary_main_frame_ || !web_contents_) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  CHECK(profile);

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  SigninBridge* signin_bridge = SigninBridgeFactory::GetForProfile(profile);
  if (!identity_manager || !signin_bridge) {
    return;
  }

  CoreAccountId primary_account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);

  if (primary_account_id.empty()) {
    signin_bridge->OpenAccountPickerBottomSheetForWebSignin(
        web_contents_.get(), destination_url,
        /*account_id=*/std::nullopt);
  } else {
    TabAndroid* tab = TabAndroid::FromWebContents(web_contents_.get());
    if (tab) {
      signin_bridge->StartUpdateCredentialsFlow(tab, destination_url,
                                                primary_account_id);
    }
  }
#else
  // TODO(crbug.com/532559920): Implement desktop re-auth and login flow for
  // enterprise proxy.
  (void)is_primary_main_frame_;
  (void)destination_url;
#endif
}
