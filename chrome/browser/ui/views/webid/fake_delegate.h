// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webid/account_selection_view.h"
#include "ui/gfx/native_widget_types.h"

class FakeDelegate : public AccountSelectionView::Delegate {
 public:
  explicit FakeDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  ~FakeDelegate() override = default;

  void OnAccountSelected(const GURL& idp_config_url,
                         const Account& account) override {}

  void OnDismiss(content::IdentityRequestDialogController::DismissReason
                     dismiss_reason) override {}
  void OnSigninToIdP() override {}
  void OnMoreDetails() override {}

  // AccountSelectionView::Delegate
  gfx::NativeView GetNativeView() override;
  content::WebContents* GetWebContents() override;

 private:
  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBID_FAKE_DELEGATE_H_
