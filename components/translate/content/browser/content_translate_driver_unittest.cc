// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/content_translate_driver.h"

#include <memory>

#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/translate/core/browser/mock_translate_client.h"
#include "components/translate/core/browser/mock_translate_ranker.h"
#include "components/translate/core/browser/translate_manager.h"
#include "components/translate/core/browser/translate_pref_names.h"
#include "components/translate/core/browser/translate_prefs.h"
#include "components/translate/core/common/language_detection_details.h"
#include "base/run_loop.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {

class MockTranslateAgent : public translate::mojom::TranslateAgent {
 public:
  MockTranslateAgent() = default;
  ~MockTranslateAgent() override = default;

  mojo::PendingRemote<translate::mojom::TranslateAgent> BindToNewPageRemote() {
    receiver_.reset();
    return receiver_.BindNewPipeAndPassRemote();
  }

  void TranslateFrame(const std::string& translate_script,
                      const std::string& source_lang,
                      const std::string& target_lang,
                      TranslateFrameCallback callback) override {
    called_translate_ = true;
    std::move(callback).Run(false, source_lang, target_lang,
                            translate::TranslateErrors::NONE);
  }

  void RevertTranslation() override {
    called_revert_ = true;
  }

#if BUILDFLAG(ENABLE_PDF)
  void PdfPageCaptured(const std::u16string& contents,
                       const std::string& pdf_lang,
                       const GURL& url) override {}
#endif

  void Disconnect() {
    receiver_.reset();
  }

  bool called_translate_ = false;
  bool called_revert_ = false;

 private:
  mojo::Receiver<translate::mojom::TranslateAgent> receiver_{this};
};

// A simple test observer that tracks whether it was notified.
class TestTranslationObserver
    : public ContentTranslateDriver::TranslationObserver {
 public:
  TestTranslationObserver() = default;
  ~TestTranslationObserver() override = default;

  void OnIsPageTranslatedChanged(content::WebContents*) override {}
  void OnTranslateEnabledChanged(content::WebContents*) override {}
};

class MockLanguageModel : public language::LanguageModel {
 public:
  std::vector<LanguageDetails> GetLanguages() override {
    return {LanguageDetails("en", 1.0)};
  }
};

class ContentTranslateDriverTest : public content::RenderViewHostTestHarness {
 public:
  ContentTranslateDriverTest() = default;
  ~ContentTranslateDriverTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    // Register sync preferences.
    language::LanguagePrefs::RegisterProfilePrefs(pref_service_.registry());
    translate::TranslatePrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kOfferTranslateEnabled, true);

    driver_ = std::make_unique<ContentTranslateDriver>(
        *web_contents(), /*url_language_histogram=*/nullptr);

    mock_translate_client_ =
        std::make_unique<translate::testing::MockTranslateClient>(
            driver_.get(), &pref_service_);

    ON_CALL(*mock_translate_client_, GetAcceptLanguagesService())
        .WillByDefault(::testing::Return(nullptr));

    translate_manager_ = std::make_unique<TranslateManager>(
        mock_translate_client_.get(), &mock_translate_ranker_,
        &mock_language_model_);

    driver_->set_translate_manager(translate_manager_.get());
  }

  void TearDown() override {
    translate_manager_.reset();
    mock_translate_client_.reset();
    driver_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<ContentTranslateDriver> driver_;
  std::unique_ptr<translate::testing::MockTranslateClient> mock_translate_client_;
  translate::testing::MockTranslateRanker mock_translate_ranker_;
  MockLanguageModel mock_language_model_;
  std::unique_ptr<TranslateManager> translate_manager_;
};

// Test that adding and removing observers works correctly.
TEST_F(ContentTranslateDriverTest, AddRemoveObserver) {
  TestTranslationObserver observer;

  driver_->AddTranslationObserver(&observer);
  driver_->RemoveTranslationObserver(&observer);

  // Should not crash when driver is destroyed.
}

// Regression test for https://crbug.com/474819145
// Verifies that destroying ContentTranslateDriver with observers still
// registered does not crash. This can happen when WebContents is destroyed
// before Java-side cleanup callbacks have a chance to remove observers.
TEST_F(ContentTranslateDriverTest, DestroyWithObserverStillRegistered) {
  TestTranslationObserver observer;

  driver_->AddTranslationObserver(&observer);

  // Intentionally NOT removing the observer before destroying the driver.
  // This simulates the race condition where WebContents destruction happens
  // before the Java-side observer cleanup.
  translate_manager_.reset();
  mock_translate_client_.reset();
  driver_.reset();

  // If we get here without crashing, the test passes.
  // Previously, this would trigger a DUMP_WILL_BE_CHECK failure in
  // ObserverList's destructor because observers were still registered.
}

// Test with multiple observers not removed.
TEST_F(ContentTranslateDriverTest, DestroyWithMultipleObservers) {
  TestTranslationObserver observer1;
  TestTranslationObserver observer2;
  TestTranslationObserver observer3;

  driver_->AddTranslationObserver(&observer1);
  driver_->AddTranslationObserver(&observer2);
  driver_->AddTranslationObserver(&observer3);

  // Remove only one observer.
  driver_->RemoveTranslationObserver(&observer2);

  // Destroy with observer1 and observer3 still registered.
  translate_manager_.reset();
  mock_translate_client_.reset();
  driver_.reset();

  // Should not crash.
}

// Test page registration with both main page and side panel agents.
TEST_F(ContentTranslateDriverTest, RegisterPageMainAndSidePanel) {
  MockTranslateAgent main_agent;
  MockTranslateAgent side_panel_agent;

  // 1. Register the standard main page first.
  translate::LanguageDetectionDetails main_details;
  main_details.url = GURL("https://example.com");
  main_details.adopted_language = "en";
  main_details.is_model_reliable = true;

  driver_->RegisterPage(main_agent.BindToNewPageRemote(), main_details, true);

  constexpr int kActiveSeqNo = 1;

  // When mimetype is NOT PDF, translating should go to main agent.
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("text/html");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(main_agent.called_translate_);
  EXPECT_FALSE(side_panel_agent.called_translate_);

  // Reset flag
  main_agent.called_translate_ = false;

  // 2. Register the side panel page using the Reading Mode host.
  translate::LanguageDetectionDetails side_panel_details;
  side_panel_details.url =
      GURL("chrome-untrusted://read-anything-side-panel.top-chrome/");
  side_panel_details.adopted_language = "en";
  side_panel_details.is_model_reliable = true;

  driver_->RegisterPage(side_panel_agent.BindToNewPageRemote(), side_panel_details,
                        true);

  // Mimetype is STILL NOT PDF, translating should still go to main agent.
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(main_agent.called_translate_);
  EXPECT_FALSE(side_panel_agent.called_translate_);

  // Reset flag
  main_agent.called_translate_ = false;

  // Change mimetype to PDF, translating should now go to side panel agent.
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("application/pdf");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(main_agent.called_translate_);
  EXPECT_TRUE(side_panel_agent.called_translate_);
}

// Test that disconnecting the side panel agent does not delete the main page agent.
TEST_F(ContentTranslateDriverTest, SidePanelDisconnectDoesNotEraseMainAgent) {
  MockTranslateAgent main_agent;
  MockTranslateAgent side_panel_agent;

  // 1. Register main page
  translate::LanguageDetectionDetails main_details;
  main_details.url = GURL("https://example.com");
  main_details.adopted_language = "en";
  main_details.is_model_reliable = true;
  driver_->RegisterPage(main_agent.BindToNewPageRemote(), main_details, true);

  constexpr int kActiveSeqNo = 1;

  // 2. Register side panel agent
  translate::LanguageDetectionDetails side_panel_details;
  side_panel_details.url =
      GURL("chrome-untrusted://read-anything-side-panel.top-chrome/");
  side_panel_details.adopted_language = "en";
  side_panel_details.is_model_reliable = true;
  driver_->RegisterPage(side_panel_agent.BindToNewPageRemote(), side_panel_details,
                        true);

  // Both should be registered and receptive.
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("text/html");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(main_agent.called_translate_);
  main_agent.called_translate_ = false;

  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("application/pdf");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(side_panel_agent.called_translate_);
  side_panel_agent.called_translate_ = false;

  // 3. Trigger disconnect on side panel agent
  side_panel_agent.Disconnect();
  base::RunLoop().RunUntilIdle();

  // Side panel should not receive translation anymore
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("application/pdf");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(side_panel_agent.called_translate_);

  // But the main agent should still receive translation (it was not deleted)!
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("text/html");
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(main_agent.called_translate_);
  main_agent.called_translate_ = false;

  // 4. Trigger disconnect on main agent
  main_agent.Disconnect();
  base::RunLoop().RunUntilIdle();

  // Now both are gone, so translating to main agent should not work either.
  driver_->TranslatePage(kActiveSeqNo, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(main_agent.called_translate_);
}

}  // namespace translate
