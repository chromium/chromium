// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_translate_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/mock_callback.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/language_detection_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::_;

constexpr char kUrl[] = "https://example.test/";

class ReadAnythingTranslateObserverTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    TranslateService::InitializeForTesting(
        net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);
    ChromeTranslateClient::CreateForWebContents(web_contents());
    // The translate driver records the determined language on the last
    // committed navigation entry, so there must be one.
    NavigateAndCommit(GURL(kUrl));
  }

  void TearDown() override {
    observer_.Reset();
    ChromeRenderViewHostTestHarness::TearDown();
    TranslateService::ShutdownForTesting();
  }

  // Creates contents with a translate client and a committed navigation, like
  // a tab's contents.
  std::unique_ptr<content::WebContents> CreateContentsWithTranslateClient() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    ChromeTranslateClient::CreateForWebContents(contents.get());
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL(kUrl));
    return contents;
  }

  void SetSourceLanguage(content::WebContents& contents,
                         const std::string& language) {
    ChromeTranslateClient::FromWebContents(&contents)
        ->GetTranslateManager()
        ->GetLanguageState()
        ->SetSourceLanguage(language);
  }

  // Makes `contents`'s translate driver notify its observers that the page's
  // language has been determined, as it does when the renderer reports it.
  void DetermineLanguage(content::WebContents& contents,
                         const std::string& language) {
    mojo::PendingRemote<translate::mojom::TranslateAgent> agent;
    translate_agent_receivers_.push_back(agent.InitWithNewPipeAndPassReceiver());
    translate::LanguageDetectionDetails details;
    details.adopted_language = language;
    ChromeTranslateClient::FromWebContents(&contents)
        ->translate_driver()
        ->RegisterPage(std::move(agent), details,
                       /*page_level_translation_criteria_met=*/false);
  }

  base::MockCallback<ReadAnythingTranslateObserver::LanguageCallback>
      language_callback_;
  ReadAnythingTranslateObserver observer_{language_callback_.Get()};

 private:
  std::vector<mojo::PendingReceiver<translate::mojom::TranslateAgent>>
      translate_agent_receivers_;
};

TEST_F(ReadAnythingTranslateObserverTest, Observe_ReportsKnownLanguage) {
  SetSourceLanguage(*web_contents(), "fr");

  EXPECT_CALL(language_callback_, Run("fr"));
  observer_.Observe(*web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_LanguageNotYetDetermined_DoesNotReport) {
  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  observer_.Observe(*web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_AlreadyObserving_ReportsLanguageAgain) {
  SetSourceLanguage(*web_contents(), "fr");
  EXPECT_CALL(language_callback_, Run("fr")).Times(2);

  observer_.Observe(*web_contents());
  observer_.Observe(*web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       LanguageDetermined_ReportsAdoptedLanguage) {
  observer_.Observe(*web_contents());

  EXPECT_CALL(language_callback_, Run("es"));
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest, Reset_StopsReportingLanguage) {
  observer_.Observe(*web_contents());
  observer_.Reset();

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_NewContents_StopsObservingPreviousContents) {
  std::unique_ptr<content::WebContents> other_contents =
      CreateContentsWithTranslateClient();
  observer_.Observe(*web_contents());
  observer_.Observe(*other_contents);

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_ContentsWithoutTranslateClient_StopsObservingPreviousContents) {
  std::unique_ptr<content::WebContents> contents_without_client =
      CreateTestWebContents();
  observer_.Observe(*web_contents());
  observer_.Observe(*contents_without_client);

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest,
       ObservedContentsDestroyed_CanObserveOtherContents) {
  std::unique_ptr<content::WebContents> other_contents =
      CreateContentsWithTranslateClient();
  observer_.Observe(*other_contents);
  // Destroying the contents destroys its translate driver, which stops the
  // observation.
  other_contents.reset();

  SetSourceLanguage(*web_contents(), "pt-br");
  EXPECT_CALL(language_callback_, Run("pt-br"));
  observer_.Observe(*web_contents());
}

}  // namespace
