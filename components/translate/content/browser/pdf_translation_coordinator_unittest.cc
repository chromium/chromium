// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/translate/content/browser/pdf_translation_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/test/test_renderer_host.h"
#include "components/pdf/browser/pdf_document_helper.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "pdf/mojom/pdf.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace translate {
namespace {

class DummyPDFDocumentHelperClient : public pdf::PDFDocumentHelperClient {
 public:
  DummyPDFDocumentHelperClient() = default;
  ~DummyPDFDocumentHelperClient() override = default;
  void OnDidScroll(const gfx::SelectionBound& start,
                   const gfx::SelectionBound& end) override {}
};

class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  ~FakePdfListener() override = default;

  void SetCaretPosition(const gfx::PointF& position) override {}
  void MoveRangeSelectionExtent(const gfx::PointF& extent) override {}
  void SetSelectionBounds(const gfx::PointF& base,
                          const gfx::PointF& extent) override {}
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

class PDFTranslationCoordinatorTest : public content::RenderViewHostTestHarness {
 protected:
  PDFTranslationCoordinatorTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    auto client = std::make_unique<DummyPDFDocumentHelperClient>();
    pdf::PDFDocumentHelper::CreateForCurrentDocument(main_rfh(), std::move(client));

    pdf_helper_ = pdf::PDFDocumentHelper::GetForCurrentDocument(main_rfh());
    ASSERT_NE(pdf_helper_, nullptr);

    PDFTranslationCoordinator::CreateForCurrentDocument(main_rfh());
    ASSERT_NE(coordinator(), nullptr);
  }

  void TearDown() override {
    pdf_helper_ = nullptr;
    content::RenderViewHostTestHarness::TearDown();
  }

  PDFTranslationCoordinator* coordinator() {
    return PDFTranslationCoordinator::GetForCurrentDocument(main_rfh());
  }

  raw_ptr<pdf::PDFDocumentHelper> pdf_helper_ = nullptr;
};

TEST_F(PDFTranslationCoordinatorTest, InitialState) {
  EXPECT_EQ(coordinator()->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kNotChecked);
  EXPECT_FALSE(coordinator()->is_translatable());
}

TEST_F(PDFTranslationCoordinatorTest, RunIfPdfIsTranslatableTranslatable) {
  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper_->SetListener(receiver.BindNewPipeAndPassRemote());

  bool callback_run = false;
  coordinator()->RunIfPdfIsTranslatable(base::BindLambdaForTesting([&]() {
    callback_run = true;
  }));
  EXPECT_FALSE(callback_run);

  // Simulate document completed loading.
  pdf_helper_->OnDocumentLoadComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_run);
  EXPECT_EQ(coordinator()->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kTranslatable);
  EXPECT_TRUE(coordinator()->is_translatable());
}

TEST_F(PDFTranslationCoordinatorTest, RunIfPdfIsTranslatableUntranslatable) {
  FakePdfListener listener;
  listener.set_has_meaningful_text(false);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper_->SetListener(receiver.BindNewPipeAndPassRemote());

  bool callback_run = false;
  coordinator()->RunIfPdfIsTranslatable(base::BindLambdaForTesting([&]() {
    callback_run = true;
  }));
  EXPECT_FALSE(callback_run);

  // Simulate document completed loading.
  pdf_helper_->OnDocumentLoadComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(callback_run);
  EXPECT_EQ(coordinator()->status(),
            PDFTranslationCoordinator::TranslatabilityStatus::kUntranslatable);
  EXPECT_FALSE(coordinator()->is_translatable());
}

TEST_F(PDFTranslationCoordinatorTest, RunIfPdfIsTranslatableMultipleCallers) {
  FakePdfListener listener;
  listener.set_has_meaningful_text(true);
  listener.set_has_javascript(false);
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_helper_->SetListener(receiver.BindNewPipeAndPassRemote());

  bool callback_run1 = false;
  bool callback_run2 = false;
  coordinator()->RunIfPdfIsTranslatable(base::BindLambdaForTesting([&]() {
    callback_run1 = true;
  }));
  coordinator()->RunIfPdfIsTranslatable(base::BindLambdaForTesting([&]() {
    callback_run2 = true;
  }));
  EXPECT_FALSE(callback_run1);
  EXPECT_FALSE(callback_run2);

  // Simulate document completed loading.
  pdf_helper_->OnDocumentLoadComplete();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_run1);
  EXPECT_TRUE(callback_run2);
}

}  // namespace
}  // namespace translate
