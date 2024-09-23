// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "ui/gfx/native_widget_types.h"

class FakeDelegate : public AccountSelectionView::Delegate {
 public:
  explicit FakeDelegate(content::WebContents* web_contents);

  ~FakeDelegate() override;

  void OnAccountSelected(const GURL& idp_config_url,
                         const Account& account) override;

  void OnDismiss(content::IdentityRequestDialogController::DismissReason
                     dismiss_reason) override {}
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url) override {}
  void OnMoreDetails() override {}
  void OnAccountsDisplayed() override {}

  using AccountSelectedCallback = base::OnceClosure;
  void SetAccountSelectedCallback(AccountSelectedCallback cb) {
    account_selected_cb_ = std::move(cb);
  }

  // AccountSelectionView::Delegate
  gfx::NativeView GetNativeView() override;
  content::WebContents* GetWebContents() override;

 private:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
  AccountSelectedCallback account_selected_cb_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_
