// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_translate_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/translate/content/browser/content_translate_driver.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/core/browser/language_state.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/common/language_detection_details.h"
#include "components/translate/core/common/translate_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/network_change_notifier.h"
#include "pdf/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PDF)
#include "pdf/pdf_features.h"
#endif  // BUILDFLAG(ENABLE_PDF)

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

  translate::LanguageState& GetLanguageState(content::WebContents& contents) {
    return *ChromeTranslateClient::FromWebContents(&contents)
                ->GetTranslateManager()
                ->GetLanguageState();
  }

  void SetSourceLanguage(content::WebContents& contents,
                         const std::string& language) {
    GetLanguageState(contents).SetSourceLanguage(language);
  }

  // Makes `contents`'s translate driver notify its observers that the page's
  // language has been determined, as it does when the renderer reports it.
  void DetermineLanguage(content::WebContents& contents,
                         const std::string& language) {
    mojo::PendingRemote<translate::mojom::TranslateAgent> agent;
    translate_agent_receivers_.push_back(
        agent.InitWithNewPipeAndPassReceiver());
    translate::LanguageDetectionDetails details;
    details.adopted_language = language;
    ChromeTranslateClient::FromWebContents(&contents)
        ->translate_driver()
        ->RegisterPage(std::move(agent), details,
                       /*page_level_translation_criteria_met=*/false);
  }

  // Makes the language state of `contents` report that it's translated, which
  // makes its translate driver notify its translation observers.
  void Translate(content::WebContents& contents) {
    translate::LanguageState& language_state = GetLanguageState(contents);
    language_state.LanguageDetermined(
        "fr", /*page_level_translation_criteria_met=*/true);
    language_state.SetCurrentLanguage("en");
  }

  base::MockCallback<ReadAnythingTranslateObserver::LanguageCallback>
      language_callback_;
  testing::NiceMock<base::MockCallback<
      ReadAnythingTranslateObserver::TranslationStateCallback>>
      translation_state_callback_;
  ReadAnythingTranslateObserver observer_{language_callback_.Get(),
                                          translation_state_callback_.Get()};

 private:
  std::vector<mojo::PendingReceiver<translate::mojom::TranslateAgent>>
      translate_agent_receivers_;
};

TEST_F(ReadAnythingTranslateObserverTest, Observe_ReportsKnownLanguage) {
  SetSourceLanguage(*web_contents(), "fr");

  EXPECT_CALL(language_callback_, Run("fr"));
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_LanguageNotYetDetermined_DoesNotReport) {
  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_AlreadyObserving_ReportsLanguageAgain) {
  SetSourceLanguage(*web_contents(), "fr");
  EXPECT_CALL(language_callback_, Run("fr")).Times(2);

  observer_.Observe(*web_contents(), *web_contents());
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       LanguageDetermined_ReportsAdoptedLanguage) {
  observer_.Observe(*web_contents(), *web_contents());

  EXPECT_CALL(language_callback_, Run("es"));
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest, Reset_StopsReportingLanguage) {
  observer_.Observe(*web_contents(), *web_contents());
  observer_.Reset();

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest,
       Observe_NewContents_StopsObservingPreviousContents) {
  std::unique_ptr<content::WebContents> other_contents =
      CreateContentsWithTranslateClient();
  observer_.Observe(*web_contents(), *web_contents());
  observer_.Observe(*other_contents, *other_contents);

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}

TEST_F(ReadAnythingTranslateObserverTest,
       ObservedContentsDestroyed_CanObserveOtherContents) {
  std::unique_ptr<content::WebContents> other_contents =
      CreateContentsWithTranslateClient();
  observer_.Observe(*other_contents, *other_contents);
  // Destroying the contents destroys its translate driver, which stops the
  // language observation. The translation state isn't observed, since
  // translate::kEnableTranslatePdf is disabled.
  other_contents.reset();

  SetSourceLanguage(*web_contents(), "pt-br");
  EXPECT_CALL(language_callback_, Run("pt-br"));
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTest,
       TranslatePdfDisabled_DoesNotReportTranslationState) {
  EXPECT_CALL(translation_state_callback_, Run(_)).Times(0);

  observer_.Observe(*web_contents(), *web_contents());
  Translate(*web_contents());
}

#if BUILDFLAG(ENABLE_PDF)
class ReadAnythingTranslateObserverOopifPdfTest
    : public ReadAnythingTranslateObserverTest {
 private:
  base::test::ScopedFeatureList feature_list_{chrome_pdf::features::kPdfOopif};
};

TEST_F(ReadAnythingTranslateObserverOopifPdfTest,
       Observe_DisplayedContentsDiffersFromTab_ObservesTabLanguage) {
  std::unique_ptr<content::WebContents> contents_without_client =
      CreateTestWebContents();
  SetSourceLanguage(*web_contents(), "fr");

  EXPECT_CALL(language_callback_, Run("fr"));
  observer_.Observe(*web_contents(), *contents_without_client);
}

// TODO(crbug.com/340272378): Remove when the GuestView PDF viewer is removed.
class ReadAnythingTranslateObserverGuestViewPdfTest
    : public ReadAnythingTranslateObserverTest {
 public:
  ReadAnythingTranslateObserverGuestViewPdfTest() {
    feature_list_.InitAndDisableFeature(chrome_pdf::features::kPdfOopif);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ReadAnythingTranslateObserverGuestViewPdfTest,
       Observe_DisplayedContentsDiffersFromTab_ObservesDisplayedLanguage) {
  // Like the inner contents of a PDF in the GuestView PDF viewer.
  std::unique_ptr<content::WebContents> pdf_contents =
      CreateContentsWithTranslateClient();
  SetSourceLanguage(*web_contents(), "fr");
  SetSourceLanguage(*pdf_contents, "de");

  EXPECT_CALL(language_callback_, Run("de"));
  observer_.Observe(*web_contents(), *pdf_contents);
}

TEST_F(ReadAnythingTranslateObserverGuestViewPdfTest,
       Observe_ContentsWithoutTranslateClient_StopsObservingPreviousContents) {
  // Like the inner contents of a PDF in the GuestView PDF viewer.
  std::unique_ptr<content::WebContents> contents_without_client =
      CreateTestWebContents();
  observer_.Observe(*web_contents(), *web_contents());
  observer_.Observe(*web_contents(), *contents_without_client);

  EXPECT_CALL(language_callback_, Run(_)).Times(0);
  DetermineLanguage(*web_contents(), "es");
}
#endif  // BUILDFLAG(ENABLE_PDF)

class ReadAnythingTranslateObserverTranslationStateTest
    : public ReadAnythingTranslateObserverTest {
 private:
  base::test::ScopedFeatureList feature_list_{translate::kEnableTranslatePdf};
};

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       Observe_ReportsNotTranslated) {
  EXPECT_CALL(translation_state_callback_, Run(false));
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       Observe_TabTranslated_ReportsTranslated) {
  Translate(*web_contents());

  EXPECT_CALL(translation_state_callback_, Run(true));
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       Observe_AlreadyObserving_DoesNotReportAgain) {
  EXPECT_CALL(translation_state_callback_, Run(false)).Times(1);

  observer_.Observe(*web_contents(), *web_contents());
  observer_.Observe(*web_contents(), *web_contents());
}

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       TranslationStateChanged_ReportsTranslationState) {
  observer_.Observe(*web_contents(), *web_contents());

  EXPECT_CALL(translation_state_callback_, Run(true));
  Translate(*web_contents());
  testing::Mock::VerifyAndClearExpectations(&translation_state_callback_);

  // Reverting to the original language, e.g. via "Show original".
  EXPECT_CALL(translation_state_callback_, Run(false));
  GetLanguageState(*web_contents()).SetCurrentLanguage("fr");
}

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       Observe_DisplayedContentsDiffersFromTab_ObservesTabTranslationState) {
  // Like the inner contents of a PDF in the GuestView PDF viewer. The
  // translation state is observed on the tab with either PDF viewer.
  std::unique_ptr<content::WebContents> contents_without_client =
      CreateTestWebContents();
  EXPECT_CALL(translation_state_callback_, Run(false));
  observer_.Observe(*web_contents(), *contents_without_client);
  testing::Mock::VerifyAndClearExpectations(&translation_state_callback_);

  EXPECT_CALL(translation_state_callback_, Run(true));
  Translate(*web_contents());
}

TEST_F(ReadAnythingTranslateObserverTranslationStateTest,
       Reset_StopsReportingTranslationState) {
  observer_.Observe(*web_contents(), *web_contents());
  observer_.Reset();

  EXPECT_CALL(translation_state_callback_, Run(_)).Times(0);
  Translate(*web_contents());
}

}  // namespace
