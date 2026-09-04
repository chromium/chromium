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
#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "pdf/buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "components/translate/content/browser/pdf_translation_coordinator.h"
#include "pdf/mojom/pdf.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#endif

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
    languages_called_count_++;
    if (on_get_languages_callback_) {
      std::move(on_get_languages_callback_).Run();
    }
    return {LanguageDetails("en", 1.0)};
  }

  int languages_called_count() const { return languages_called_count_; }
  void reset_called_count() { languages_called_count_ = 0; }
  void set_on_get_languages_callback(base::OnceClosure callback) {
    on_get_languages_callback_ = std::move(callback);
  }

 private:
  int languages_called_count_ = 0;
  base::OnceClosure on_get_languages_callback_;
};

#if BUILDFLAG(ENABLE_PDF)
class DummyPDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  DummyPDFDocumentHelperClient() = default;
  ~DummyPDFDocumentHelperClient() override = default;
};

class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  ~FakePdfListener() override = default;

  void SetCaretPosition(const gfx::PointF& position) override {}
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {}
  void SetSelectionBase(const gfx::PointF& base) override {}
  void GetPdfBytes(uint32_t size_limit, GetPdfBytesCallback callback) override {
    std::move(callback).Run(pdf::mojom::PdfListener::GetPdfBytesStatus::kFailed,
                            std::vector<uint8_t>(), 0);
  }
  void GetPageText(int32_t page_index, GetPageTextCallback callback) override {
    std::move(callback).Run(std::u16string());
  }
  void GetMostVisiblePageIndex(
      GetMostVisiblePageIndexCallback callback) override {
    std::move(callback).Run(std::nullopt);
  }
  void HasMeaningfulText(HasMeaningfulTextCallback callback) override {
    std::move(callback).Run(has_meaningful_text_);
  }
  void HasJavaScript(HasJavaScriptCallback callback) override {
    std::move(callback).Run(has_javascript_);
  }
  void IsPasswordProtected(IsPasswordProtectedCallback callback) override {
    std::move(callback).Run(is_password_protected_);
  }
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  void GetSaveDataBufferHandlerForDrive(
      pdf::mojom::SaveRequestType request_type,
      GetSaveDataBufferHandlerForDriveCallback callback) override {
    std::move(callback).Run(nullptr);
  }
#endif

  void set_has_meaningful_text(bool has_meaningful_text) {
    has_meaningful_text_ = has_meaningful_text;
  }
  void set_has_javascript(bool has_javascript) {
    has_javascript_ = has_javascript;
  }
  void set_is_password_protected(bool is_password_protected) {
    is_password_protected_ = is_password_protected;
  }

 private:
  bool has_meaningful_text_ = false;
  bool has_javascript_ = false;
  bool is_password_protected_ = false;
};
#endif  // BUILDFLAG(ENABLE_PDF)

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
    if (driver_) {
      driver_->set_translate_manager(nullptr);
    }
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
  driver_->set_translate_manager(nullptr);
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
  driver_->set_translate_manager(nullptr);
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

// Test that reloading a page preserves the side panel agent and updates its
// sequence number mapping cleanly.
TEST_F(ContentTranslateDriverTest, PageReloadPreservesSidePanelAgent) {
  MockTranslateAgent main_agent1;
  MockTranslateAgent side_panel_agent;

  // 1. Register main page first.
  translate::LanguageDetectionDetails main_details1;
  main_details1.url = GURL("https://example.com");
  main_details1.adopted_language = "en";
  main_details1.is_model_reliable = true;
  driver_->RegisterPage(main_agent1.BindToNewPageRemote(), main_details1, true);

  // Active page sequence number is now 1.
  constexpr int kSeqNo1 = 1;

  // 2. Register side panel agent.
  translate::LanguageDetectionDetails side_panel_details;
  side_panel_details.url =
      GURL("chrome-untrusted://read-anything-side-panel.top-chrome/");
  side_panel_details.adopted_language = "en";
  side_panel_details.is_model_reliable = true;
  driver_->RegisterPage(side_panel_agent.BindToNewPageRemote(), side_panel_details,
                        true);

  // Both agents are registered under sequence number 1.
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("application/pdf");
  driver_->TranslatePage(kSeqNo1, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(side_panel_agent.called_translate_);
  side_panel_agent.called_translate_ = false;

  // 3. Navigate/Reload to same or new URL. This changes the MainFrame PageUkmSourceId.
  NavigateAndCommit(GURL("https://example.com/reloaded"));

  // 4. Register the new page.
  MockTranslateAgent main_agent2;
  translate::LanguageDetectionDetails main_details2;
  main_details2.url = GURL("https://example.com/reloaded");
  main_details2.adopted_language = "en";
  main_details2.is_model_reliable = true;
  driver_->RegisterPage(main_agent2.BindToNewPageRemote(), main_details2, true);

  // The active page sequence number is updated to 2 (kSeqNo2).
  constexpr int kSeqNo2 = 2;

  // 5. Translating with the new sequence number kSeqNo2 under PDF should
  // correctly invoke the moved/preserved side panel agent.
  content::WebContentsTester::For(web_contents())->SetMainFrameMimeType("application/pdf");
  driver_->TranslatePage(kSeqNo2, "script", "en", "fr");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(side_panel_agent.called_translate_);
}

// Verifies that RegisterPage returns early and safely when web_contents() is null.
TEST_F(ContentTranslateDriverTest, RegisterPageWithNullWebContents) {
  MockTranslateAgent agent;
  translate::LanguageDetectionDetails details;
  details.url = GURL("https://example.com");
  details.adopted_language = "fr";
  details.is_model_reliable = true;

  DeleteContents();
  ASSERT_EQ(driver_->web_contents(), nullptr);

  // RegisterPage should return early when web_contents() is null without crashing.
  driver_->RegisterPage(agent.BindToNewPageRemote(), details, true);

  // Translation should not have been initiated.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 0);
}

// Verifies that when PDF translation feature is disabled, RegisterPage on a PDF
// does not trigger the PDF translatability check via PDFTranslationCoordinator,
// but directly initiates translation.
TEST_F(ContentTranslateDriverTest, RegisterPdfPageWhenPdfTranslationDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(translate::kEnableTranslatePdf);

  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType("application/pdf");

  MockTranslateAgent agent;
  translate::LanguageDetectionDetails details;
  details.url = GURL("https://example.com/test.pdf");
  details.adopted_language = "fr";
  details.is_model_reliable = true;

  driver_->RegisterPage(agent.BindToNewPageRemote(), details, true);

  // When kEnableTranslatePdf is disabled, IsPdfTranslation() is false, so
  // InitiateTranslation is called synchronously during RegisterPage.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 1);
#if BUILDFLAG(ENABLE_PDF)
  EXPECT_EQ(PDFTranslationCoordinator::GetForCurrentDocument(
                web_contents()->GetPrimaryMainFrame()),
            nullptr);
#endif
}

// Verifies that when a non-PDF page is registered with the PDF translation
// feature enabled, IsPdfTranslation() is false and translation is initiated
// directly without using PDFTranslationCoordinator.
TEST_F(ContentTranslateDriverTest, RegisterNonPdfPageWhenPdfTranslationEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(translate::kEnableTranslatePdf);

  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType("text/html");

  MockTranslateAgent agent;
  translate::LanguageDetectionDetails details;
  details.url = GURL("https://example.com/page.html");
  details.adopted_language = "fr";
  details.is_model_reliable = true;

  driver_->RegisterPage(agent.BindToNewPageRemote(), details, true);

  // For non-PDF pages, IsPdfTranslation() is false, so InitiateTranslation
  // is called synchronously during RegisterPage.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 1);
#if BUILDFLAG(ENABLE_PDF)
  EXPECT_EQ(PDFTranslationCoordinator::GetForCurrentDocument(
                web_contents()->GetPrimaryMainFrame()),
            nullptr);
#endif
}

#if BUILDFLAG(ENABLE_PDF)
// Tests for PDF translation eligibility coordination in ContentTranslateDriver.
class ContentTranslateDriverPdfTest : public ContentTranslateDriverTest {
 public:
  ContentTranslateDriverPdfTest() {
    scoped_feature_list_.InitAndEnableFeature(translate::kEnableTranslatePdf);
  }

  void SetUp() override {
    ContentTranslateDriverTest::SetUp();
    NavigateAndCommit(GURL("https://example.com/test.pdf"));
    content::WebContentsTester::For(web_contents())
        ->SetMainFrameMimeType("application/pdf");

    pdf::PDFDocumentHelper::CreateForCurrentDocument(
        main_rfh(), std::make_unique<DummyPDFDocumentHelperClient>());
    pdf_helper_ = pdf::PDFDocumentHelper::GetForCurrentDocument(main_rfh());
  }

  void TearDown() override {
    pdf_helper_ = nullptr;
    ContentTranslateDriverTest::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<pdf::PDFDocumentHelper> pdf_helper_ = nullptr;
};

// Verifies that for an eligible translatable PDF, RegisterPage defers translation
// initiation until the translatability check passes.
TEST_F(ContentTranslateDriverPdfTest, RegisterPdfPageTranslatable) {
  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper_->SetListener(receiver.BindNewPipeAndPassRemote());

  MockTranslateAgent agent;
  translate::LanguageDetectionDetails details;
  details.url = GURL("https://example.com/test.pdf");
  details.adopted_language = "fr";
  details.is_model_reliable = true;

  driver_->RegisterPage(agent.BindToNewPageRemote(), details, true);

  auto* coordinator =
      PDFTranslationCoordinator::GetForCurrentDocument(main_rfh());
  ASSERT_NE(coordinator, nullptr);
  EXPECT_EQ(coordinator->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kNotChecked);
  // Translation should NOT be initiated yet before translatability check completes.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 0);

  // Complete document loading and wait for translation initiation to be called.
  base::RunLoop run_loop;
  mock_language_model_.set_on_get_languages_callback(run_loop.QuitClosure());
  pdf_helper_->OnDocumentLoadComplete();
  run_loop.Run();

  EXPECT_EQ(coordinator->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kTranslatable);
  // Translation should now have been initiated.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 1);
  EXPECT_EQ(translate_manager_->GetLanguageState()->pdf_translatability_status(),
            translate::LanguageState::PdfTranslatabilityStatus::kTranslatable);
}

// Verifies that for an untranslatable PDF, RegisterPage does NOT initiate translation,
// sets the PDF translatability status to untranslatable, and updates page-level
// translation criteria.
TEST_F(ContentTranslateDriverPdfTest, RegisterPdfPageUntranslatable) {
  FakePdfListener listener;
  listener.set_has_meaningful_text(false);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper_->SetListener(receiver.BindNewPipeAndPassRemote());

  MockTranslateAgent agent;
  translate::LanguageDetectionDetails details;
  details.url = GURL("https://example.com/test.pdf");
  details.adopted_language = "fr";
  details.is_model_reliable = true;

  driver_->RegisterPage(agent.BindToNewPageRemote(), details, true);

  auto* coordinator =
      PDFTranslationCoordinator::GetForCurrentDocument(main_rfh());
  ASSERT_NE(coordinator, nullptr);
  EXPECT_EQ(coordinator->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kNotChecked);
  EXPECT_EQ(mock_language_model_.languages_called_count(), 0);

  // Complete document loading and flush Mojo IPC to ensure translatability
  // checks and coordinator updates complete.
  pdf_helper_->OnDocumentLoadComplete();
  receiver.FlushForTesting();
  receiver.FlushForTesting();

  EXPECT_EQ(coordinator->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kUntranslatable);
  // Untranslatable PDF must not initiate translation.
  EXPECT_EQ(mock_language_model_.languages_called_count(), 0);
  EXPECT_EQ(translate_manager_->GetLanguageState()->pdf_translatability_status(),
            translate::LanguageState::PdfTranslatabilityStatus::kUntranslatable);
  EXPECT_FALSE(
      translate_manager_->GetLanguageState()->page_level_translation_criteria_met());
}
#endif  // BUILDFLAG(ENABLE_PDF)

}  // namespace translate
