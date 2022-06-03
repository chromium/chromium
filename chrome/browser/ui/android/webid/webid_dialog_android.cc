// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ui/webid/webid_dialog.h"

namespace content {
class WebContents;
}  // namespace content

// A stub implementation for WebID Dialog for Android.
//
// The dialog simply accepts various permissions and shows no UI. This is here
// to avoid crash when WebID is used on Android.
class WebIdDialogAndroidStub : public WebIdDialog {
 public:
  explicit WebIdDialogAndroidStub(content::WebContents* rp_web_contents,
                                  CloseCallback callback)
      : WebIdDialog(rp_web_contents), close_callback_(std::move(callback)) {}

  WebIdDialogAndroidStub(const WebIdDialogAndroidStub&) = delete;
  WebIdDialogAndroidStub operator=(const WebIdDialogAndroidStub&) = delete;
  ~WebIdDialogAndroidStub() override {}

  void ShowInitialPermission(const std::u16string& idp_hostname,
                             const std::u16string& rp_hostname,
                             PermissionDialogMode mode,
                             PermissionCallback callback) override {
    // Post a task to run the callback because FederatedAuthNavigationThrottle
    // currently assumes these callbacks are run asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UserApproval::kApproved));
  }
  void ShowTokenExchangePermission(const std::u16string& idp_hostname,
                                   const std::u16string& rp_hostname,
                                   PermissionCallback callback) override {
    // Post a task to run the callback because FederatedAuthNavigationThrottle
    // currently assumes these callbacks are run asynchronously.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), UserApproval::kApproved));
    delete this;
  }
  void ShowSigninPage(content::WebContents* idp_web_contents,
                      const GURL& idp_signin_url) override {}
  void CloseSigninPage() override { std::move(close_callback_).Run(); }

 private:
  CloseCallback close_callback_;
};

// static
WebIdDialog* WebIdDialog::Create(content::WebContents* rp_web_contents,
                                 CloseCallback callback) {
  // This instance gets deleted once token exchange permission is called.
  return new WebIdDialogAndroidStub(rp_web_contents, std::move(callback));
}
