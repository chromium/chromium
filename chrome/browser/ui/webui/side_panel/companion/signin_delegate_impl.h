// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_SIGNIN_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_SIGNIN_DELEGATE_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/companion/core/signin_delegate.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace companion {

class SigninDelegateImpl : public SigninDelegate {
 public:
  explicit SigninDelegateImpl(content::WebContents* webui_contents);
  ~SigninDelegateImpl() override;

  // Disallow copy/assign.
  SigninDelegateImpl(const SigninDelegateImpl&) = delete;
  SigninDelegateImpl& operator=(const SigninDelegateImpl&) = delete;

  // SigninDelegate implementation.
  bool AllowedSignin() override;
  bool IsSignedIn() override;
  void StartSigninFlow() override;
  void EnableMsbb(bool enable_msbb) override;
  void OpenUrlInBrowser(const GURL& url, bool use_new_tab) override;
  bool ShouldShowRegionSearchIPH() override;

 private:
  Profile* GetProfile();

  // The WebContents associated with the companion page.
  raw_ptr<content::WebContents> webui_contents_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_SIGNIN_DELEGATE_IMPL_H_
