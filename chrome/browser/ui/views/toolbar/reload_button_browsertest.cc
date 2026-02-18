// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/reload_button.h"

#include "build/build_config.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/polling_state_observer.h"
#include "ui/views/interaction/element_tracker_views.h"

class ReloadButtonBrowserTest : public InteractiveBrowserTest,
                                public testing::WithParamInterface<bool> {
 public:
  ReloadButtonBrowserTest() {
    std::vector<base::test::FeatureRef> webui_features = {
        features::kInitialWebUI, features::kWebUIReloadButton};
    if (GetParam()) {
      feature_list_.InitWithFeatures(webui_features, {});
    } else {
      feature_list_.InitWithFeatures({}, webui_features);
    }
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

namespace {

class FakeProtocolHandlerDelegate : public ExternalProtocolHandler::Delegate {
 public:
  FakeProtocolHandlerDelegate() = default;
  FakeProtocolHandlerDelegate(const FakeProtocolHandlerDelegate&) = delete;
  FakeProtocolHandlerDelegate& operator=(const FakeProtocolHandlerDelegate&) =
      delete;

  void WaitForCall() { run_loop_.Run(); }

  scoped_refptr<shell_integration::DefaultSchemeClientWorker> CreateShellWorker(
      const GURL& url) override {
    return base::MakeRefCounted<shell_integration::DefaultSchemeClientWorker>(
        url);
  }

  ExternalProtocolHandler::BlockState GetBlockState(const std::string& scheme,
                                                    Profile* profile) override {
    return ExternalProtocolHandler::GetBlockState(scheme, nullptr, profile);
  }

  void BlockRequest() override { run_loop_.Quit(); }

  void RunExternalProtocolDialog(
      const GURL& url,
      content::WebContents* web_contents,
      ui::PageTransition page_transition,
      bool has_user_gesture,
      const std::optional<url::Origin>& initiating_origin,
      const std::u16string& program_name) override {
    run_loop_.Quit();
  }

  void LaunchUrlWithoutSecurityCheck(
      const GURL& url,
      content::WebContents* web_contents) override {
    run_loop_.Quit();
  }

  void FinishedProcessingCheck() override { run_loop_.Quit(); }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// TODO(crbug.com/40853146): Fix flakiness on Win and Mac.
// TODO(crbug.com/41481789): Fix consistent failing on Linux.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#define MAYBE_AllowExternalProtocols DISABLED_AllowExternalProtocols
#else
#define MAYBE_AllowExternalProtocols AllowExternalProtocols
#endif
IN_PROC_BROWSER_TEST_P(ReloadButtonBrowserTest, MAYBE_AllowExternalProtocols) {
  const char fake_protocol[] = "fake";

  // Call LaunchUrl once to trigger the blocked state.
  GURL url("fake://example.test");

  using BlockState = ExternalProtocolHandler::BlockState;
  using BlockStateObserver = ui::test::PollingStateObserver<BlockState>;
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(BlockStateObserver, kBlockState);

  FakeProtocolHandlerDelegate delegate;
  ExternalProtocolHandler::SetDelegateForTesting(&delegate);
  ExternalProtocolHandler::LaunchUrl(
      url,
      base::BindRepeating(&ReloadButtonBrowserTest::GetWebContents,
                          base::Unretained(this)),
      ui::PAGE_TRANSITION_LINK, /*has_user_gesture=*/true,
      /*is_in_fenced_frame_tree=*/false, url::Origin::Create(url),
      content::WeakDocumentPtr());
  delegate.WaitForCall();
  ExternalProtocolHandler::SetDelegateForTesting(nullptr);

  ASSERT_EQ(ExternalProtocolHandler::BLOCK,
            ExternalProtocolHandler::GetBlockState(fake_protocol, nullptr,
                                                   browser()->profile()));

  ASSERT_TRUE(RunTestSequence(
      // Clicking the reload button should remove the blocked state.
      MoveMouseTo(kReloadButtonElementId,
                  base::BindOnce([](ui::TrackedElement* el) {
                    return el->GetScreenBounds().CenterPoint();
                  })),
      ClickMouse(),
      // For the Native implementation, the state change is synchronous and
      // happens immediately during ClickMouse().
      // For the WebUI implementation, the state change is asynchronous,
      // so we must poll until the message arrives and the state updates.
      PollStateUntil(
          kBlockState,
          [this, &fake_protocol]() {
            return ExternalProtocolHandler::GetBlockState(
                fake_protocol, nullptr, browser()->profile());
          },
          testing::Ne(ExternalProtocolHandler::BLOCK))));
}

INSTANTIATE_TEST_SUITE_P(All, ReloadButtonBrowserTest, testing::Bool());
