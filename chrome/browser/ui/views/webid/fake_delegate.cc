// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fake_delegate.h"

FakeDelegate::FakeDelegate(content::WebContents* web_contents)
    : web_contents_(web_contents) {}

FakeDelegate::~FakeDelegate() = default;

void FakeDelegate::OnAccountSelected(const GURL& idp_config_url,
                                     const Account& account) {
  if (account_selected_cb_) {
    std::move(account_selected_cb_).Run();
  }
}

gfx::NativeView FakeDelegate::GetNativeView() {
  return gfx::NativeView();
}

content::WebContents* FakeDelegate::GetWebContents() {
  return web_contents_;
}
