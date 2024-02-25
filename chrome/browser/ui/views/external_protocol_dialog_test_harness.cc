// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/external_protocol_dialog_test_harness.h"

#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/ui/browser.h"
#include "net/dns/mock_host_resolver.h"

namespace test {

class ExternalProtocolDialogTestApi {
 public:
  explicit ExternalProtocolDialogTestApi(ExternalProtocolDialog* dialog)
      : dialog_(dialog) {}

  ExternalProtocolDialogTestApi(const ExternalProtocolDialogTestApi&) = delete;
  ExternalProtocolDialogTestApi& operator=(
      const ExternalProtocolDialogTestApi&) = delete;

  void SetCheckBoxSelected(bool checked) {
    dialog_->SetRememberSelectionCheckboxCheckedForTesting(checked);
  }

 private:
  raw_ptr<ExternalProtocolDialog> dialog_;
};

}  // namespace test

namespace {

constexpr char kInitiatingOrigin[] = "a.test";
constexpr char kRedirectingOrigin[] = "b.test";

class FakeDefaultSchemeClientWorker
    : public shell_integration::DefaultSchemeClientWorker {
 public:
  explicit FakeDefaultSchemeClientWorker(const GURL& url)
      : DefaultSchemeClientWorker(url) {}
  FakeDefaultSchemeClientWorker(const FakeDefaultSchemeClientWorker&) = delete;
  FakeDefaultSchemeClientWorker& operator=(
      const FakeDefaultSchemeClientWorker&) = delete;

 private:
  ~FakeDefaultSchemeClientWorker() override = default;
  shell_integration::DefaultWebClientState CheckIsDefaultImpl() override {
    return shell_integration::DefaultWebClientState::NOT_DEFAULT;
  }

  std::u16string GetDefaultClientNameImpl() override { return u"TestApp"; }

  void SetAsDefaultImpl(base::OnceClosure on_finished_callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_finished_callback));
  }
};

}  // namespace

ExternalProtocolDialogBrowserTest::ExternalProtocolDialogBrowserTest() {
  ExternalProtocolHandler::SetDelegateForTesting(this);
}

ExternalProtocolDialogBrowserTest::~ExternalProtocolDialogBrowserTest() {
  ExternalProtocolHandler::SetDelegateForTesting(nullptr);
}

void ExternalProtocolDialogBrowserTest::ShowUi(
    const std::string& initiating_origin) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  dialog_ = new ExternalProtocolDialog(
      web_contents, GURL("telnet://12345"), u"/usr/bin/telnet",
      url::Origin::Create(GURL(initiating_origin)),
      web_contents->GetPrimaryMainFrame()->GetWeakDocumentPtr());
}

void ExternalProtocolDialogBrowserTest::SetChecked(bool checked) {
  test::ExternalProtocolDialogTestApi(dialog_).SetCheckBoxSelected(checked);
}

// ExternalProtocolHandler::Delegate:
scoped_refptr<shell_integration::DefaultSchemeClientWorker>
ExternalProtocolDialogBrowserTest::CreateShellWorker(const GURL& url) {
  return base::MakeRefCounted<FakeDefaultSchemeClientWorker>(url);
}

ExternalProtocolHandler::BlockState
ExternalProtocolDialogBrowserTest::GetBlockState(const std::string& scheme,
                                                 Profile* profile) {
  return ExternalProtocolHandler::UNKNOWN;
}

void ExternalProtocolDialogBrowserTest::RunExternalProtocolDialog(
    const GURL& url,
    content::WebContents* web_contents,
    ui::PageTransition page_transition,
    bool has_user_gesture,
    const std::optional<url::Origin>& initiating_origin,
    const std::u16string& program_name) {
  EXPECT_EQ(program_name, u"TestApp");
  url_did_launch_ = true;
  launch_url_ = initiating_origin->host();
  if (launch_url_run_loop_) {
    launch_url_run_loop_->Quit();
  }
}

void ExternalProtocolDialogBrowserTest::LaunchUrlWithoutSecurityCheck(
    const GURL& url,
    content::WebContents* web_contents) {
  url_did_launch_ = true;
}

void ExternalProtocolDialogBrowserTest::OnSetBlockState(
    const std::string& scheme,
    const url::Origin& initiating_origin,
    BlockState state) {
  blocked_scheme_ = scheme;
  blocked_origin_ = initiating_origin;
  blocked_state_ = state;
}

void ExternalProtocolDialogBrowserTest::SetUpOnMainThread() {
  DialogBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule(kInitiatingOrigin, "127.0.0.1");
  host_resolver()->AddRule(kRedirectingOrigin, "127.0.0.1");
}

void ExternalProtocolDialogBrowserTest::WaitForLaunchUrl() {
  if (url_did_launch_) {
    return;
  }
  launch_url_run_loop_ = std::make_unique<base::RunLoop>();
  launch_url_run_loop_->Run();
}
