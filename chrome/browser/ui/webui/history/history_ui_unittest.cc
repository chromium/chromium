// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui.h"

#include <memory>
#include <vector>

#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/ui/webui/history/browsing_history_handler.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_education/webui/user_education.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "ui/webui/resources/cr_components/history/history_cross_device_signin_promo.mojom.h"

namespace {

class HistoryUITest : public testing::Test {
 public:
  HistoryUITest() = default;
  ~HistoryUITest() override = default;

  content::TestWebUI* web_ui() { return &web_ui_; }

 protected:
  void SetUp() override {
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    web_ui_.set_web_contents(web_contents_.get());
  }

  void TearDown() override {
    web_ui_.set_web_contents(nullptr);
    web_contents_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  content::TestWebUI web_ui_;
};

TEST_F(HistoryUITest, InstantiationAndBindPageHandler) {
  auto history_ui = std::make_unique<HistoryUI>(web_ui());
  mojo::Remote<history::mojom::PageHandler> handler_remote;
  history_ui->BindInterface(handler_remote.BindNewPipeAndPassReceiver());
  EXPECT_NE(history_ui->GetBrowsingHistoryHandlerForTesting(), nullptr);
}

#if BUILDFLAG(IS_ANDROID)
class FakeForeignSessionPage : public history::mojom::ForeignSessionPage {
 public:
  FakeForeignSessionPage() = default;
  ~FakeForeignSessionPage() override = default;

  mojo::PendingRemote<history::mojom::ForeignSessionPage> BindAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void OnForeignSessionsChanged(
      std::vector<history::mojom::ForeignSessionPtr> sessions) override {}

 private:
  mojo::Receiver<history::mojom::ForeignSessionPage> receiver_{this};
};

TEST_F(HistoryUITest, AndroidForeignSessionHandlerReturnsEmpty) {
  auto history_ui = std::make_unique<HistoryUI>(web_ui());
  mojo::Remote<history::mojom::ForeignSessionPageHandlerFactory> factory_remote;
  history_ui->BindInterface(factory_remote.BindNewPipeAndPassReceiver());

  FakeForeignSessionPage page;
  mojo::Remote<history::mojom::ForeignSessionPageHandler> handler_remote;
  factory_remote->CreateForeignSessionPageHandler(
      page.BindAndPassRemote(), handler_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<std::vector<history::mojom::ForeignSessionPtr>> future;
  handler_remote->GetForeignSessions(future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

TEST_F(HistoryUITest, AndroidSigninPromoCardReturnsFalse) {
  auto history_ui = std::make_unique<HistoryUI>(web_ui());
  mojo::Remote<history_cross_device_signin_promo::mojom::
                   HistoryCrossDeviceSigninPromoHandler>
      promo_remote;
  history_ui->BindInterface(promo_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future;
  promo_remote->ShouldShowPromoCard(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(HistoryUITest, AndroidUserEducationMixedTrustHandler) {
  auto history_ui = std::make_unique<HistoryUI>(web_ui());
  mojo::Remote<user_education::mojom::UserEducationMixedTrustHandlerFactory>
      factory_remote;
  history_ui->BindInterface(factory_remote.BindNewPipeAndPassReceiver());

  mojo::Remote<user_education::mojom::UserEducationMixedTrustHandler>
      handler_remote;
  factory_remote->CreateUserEducationMixedTrustHandler(
      handler_remote.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> future;
  handler_remote->MaybeShowNewBadgeFor("feature_name", future.GetCallback());
  EXPECT_FALSE(future.Get());
}

TEST_F(HistoryUITest, OpenClearBrowsingDataWithNullNativeWindowDoesNotCrash) {
  auto history_ui = std::make_unique<HistoryUI>(web_ui());
  mojo::Remote<history::mojom::PageHandler> handler_remote;
  history_ui->BindInterface(handler_remote.BindNewPipeAndPassReceiver());
  ASSERT_NE(history_ui->GetBrowsingHistoryHandlerForTesting(), nullptr);
  // WebContents created in test fixture has no NativeWindow attached;
  // verify OpenClearBrowsingDataDialog handles null window safely without
  // crashing.
  history_ui->GetBrowsingHistoryHandlerForTesting()
      ->OpenClearBrowsingDataDialog();
}
#endif

}  // namespace
