// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_document_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/pdf/browser/pdf_document_helper_client.h"
#include "components/pdf/common/pdf_accessibility.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "pdf/mojom/pdf.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_editing_controller.h"

namespace pdf {

namespace {

using ::testing::NiceMock;

class FakePdfListener : public pdf::mojom::PdfListener {
 public:
  FakePdfListener() = default;
  FakePdfListener(const FakePdfListener&) = delete;
  FakePdfListener& operator=(const FakePdfListener&) = delete;
  ~FakePdfListener() override = default;

  MOCK_METHOD(void, SetCaretPosition, (const gfx::PointF&), (override));
  MOCK_METHOD(void, MoveRangeSelectionExtent, (const gfx::PointF&), (override));
  MOCK_METHOD(void, SetSelectionBase, (const gfx::PointF&), (override));
  MOCK_METHOD(void,
              GetPdfBytes,
              (uint32_t, GetPdfBytesCallback callback),
              (override));
  MOCK_METHOD(void,
              GetPageText,
              (int32_t, GetPageTextCallback callback),
              (override));
  MOCK_METHOD(void,
              GetMostVisiblePageIndex,
              (GetMostVisiblePageIndexCallback callback),
              (override));
  MOCK_METHOD(void,
              HasMeaningfulText,
              (HasMeaningfulTextCallback callback),
              (override));
  MOCK_METHOD(void,
              HasJavaScript,
              (HasJavaScriptCallback callback),
              (override));
  MOCK_METHOD(void,
              IsPasswordProtected,
              (IsPasswordProtectedCallback callback),
              (override));
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
  MOCK_METHOD(void,
              GetSaveDataBufferHandlerForDrive,
              (pdf::mojom::SaveRequestType,
               GetSaveDataBufferHandlerForDriveCallback callback),
              (override));
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
};

class FakePdfAccessibilityImageProvider
    : public pdf::mojom::PdfAccessibilityImageProvider {
 public:
  FakePdfAccessibilityImageProvider() = default;
  FakePdfAccessibilityImageProvider(const FakePdfAccessibilityImageProvider&) =
      delete;
  FakePdfAccessibilityImageProvider& operator=(
      const FakePdfAccessibilityImageProvider&) = delete;
  ~FakePdfAccessibilityImageProvider() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.reset();
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<
            pdf::mojom::PdfAccessibilityImageProvider>(std::move(handle)));
  }

  void ResetReceiver() { receiver_.reset(); }

  base::WeakPtr<FakePdfAccessibilityImageProvider> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  MOCK_METHOD(void,
              GetImageForNode,
              (int32_t, GetImageForNodeCallback callback),
              (override));

 private:
  mojo::AssociatedReceiver<pdf::mojom::PdfAccessibilityImageProvider> receiver_{
      this};
  base::WeakPtrFactory<FakePdfAccessibilityImageProvider> weak_ptr_factory_{
      this};
};

class TestPDFDocumentHelperClient : public PDFDocumentHelperClient {
 public:
  TestPDFDocumentHelperClient() = default;
  TestPDFDocumentHelperClient(const TestPDFDocumentHelperClient&) = delete;
  TestPDFDocumentHelperClient& operator=(const TestPDFDocumentHelperClient&) =
      delete;
  ~TestPDFDocumentHelperClient() override = default;

  const gfx::SelectionBound& GetSelectionBoundStart() const { return start_; }
  const gfx::SelectionBound& GetSelectionBoundEnd() const { return end_; }

 private:
  // PDFDocumentHelperClient:
  void OnDidScroll(const gfx::SelectionBound& start,
                   const gfx::SelectionBound& end) override {
    start_ = start;
    end_ = end;
  }

 private:
  // The last bounds reported by PDFDocumentHelper.
  gfx::SelectionBound start_;
  gfx::SelectionBound end_;
};

}  // namespace

class PDFDocumentHelperTest : public content::ContentBrowserTest {
 public:
  PDFDocumentHelperTest() = default;
  ~PDFDocumentHelperTest() override = default;

 protected:
  void SelectionChanged(const gfx::PointF& left,
                        int32_t left_height,
                        const gfx::PointF& right,
                        int32_t right_height) {
    pdf_document_helper()->SelectionChanged(left, left_height, right,
                                            right_height);
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  PDFDocumentHelper* pdf_document_helper() {
    return PDFDocumentHelper::GetForCurrentDocument(GetPrimaryMainFrame());
  }

  content::RenderWidgetHostView* GetRenderWidgetHostView() {
    return shell()->web_contents()->GetRenderWidgetHostView();
  }

  TestPDFDocumentHelperClient* client() { return client_; }

  void SetAccessibilityImageProvider(
      FakePdfAccessibilityImageProvider& image_provider) {
    GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            pdf::mojom::PdfAccessibilityImageProvider::Name_,
            base::BindRepeating(
                &FakePdfAccessibilityImageProvider::BindReceiver,
                image_provider.GetWeakPtr()));
  }

  // content::ContentBrowserTest:
  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(content::NavigateToURL(shell(), GURL("about:blank")));
    auto client = std::make_unique<TestPDFDocumentHelperClient>();
    client_ = client.get();
    PDFDocumentHelper::CreateForCurrentDocument(GetPrimaryMainFrame(),
                                                std::move(client));
  }

  void TearDownOnMainThread() override {
    GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces()
        ->OverrideBinderForTesting(
            pdf::mojom::PdfAccessibilityImageProvider::Name_,
            base::NullCallback());
    client_ = nullptr;
    content::ContentBrowserTest::TearDownOnMainThread();
  }

 private:
  raw_ptr<TestPDFDocumentHelperClient> client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, SetListenerTwice) {
  NiceMock<FakePdfListener> listener;

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }

  {
    mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
    pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());
  }
}

// Tests that select-changed on a pdf text brings up selection handles and the
// quick menu in the reasonable position.
IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, SelectionChanged) {
  gfx::SelectionBound initial_start = client()->GetSelectionBoundStart();
  gfx::SelectionBound initial_end = client()->GetSelectionBoundEnd();

  EXPECT_EQ(gfx::RectF(),
            gfx::RectFBetweenSelectionBounds(initial_start, initial_end));
  EXPECT_EQ(gfx::RectF(), gfx::RectFBetweenVisibleSelectionBounds(initial_start,
                                                                  initial_end));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  gfx::SelectionBound start = client()->GetSelectionBoundStart();
  gfx::SelectionBound end = client()->GetSelectionBoundEnd();

#if BUILDFLAG(IS_MAC)
  // Since macOS does not support Touch Selection Editing, the
  // SelectionChanged() call does not affect the selection bounds.
  EXPECT_EQ(start, initial_start);
  EXPECT_EQ(end, initial_end);
#else
  gfx::PointF origin_f;
  content::RenderWidgetHostView* view = GetRenderWidgetHostView();
  if (view) {
    origin_f = view->TransformPointToRootCoordSpaceF(gfx::PointF());
  }

  gfx::SelectionBound expected_start;
  {
    gfx::PointF edge_start(kLeft.x() + origin_f.x(), kLeft.y() + origin_f.y());
    gfx::PointF edge_end(kLeft.x() + origin_f.x(),
                         kLeft.y() + origin_f.y() + kLeftHeight);
    expected_start.SetEdge(edge_start, edge_end);
    expected_start.SetVisibleEdge(edge_start, edge_end);
  }

  gfx::SelectionBound expected_end;
  {
    gfx::PointF edge_start(kRight.x() + origin_f.x(),
                           kRight.y() + origin_f.y());
    gfx::PointF edge_end(kRight.x() + origin_f.x(),
                         kRight.y() + origin_f.y() + kRightHeight);
    expected_end.SetEdge(edge_start, edge_end);
    expected_end.SetVisibleEdge(edge_start, edge_end);
  }

  ASSERT_NE(expected_start, expected_end);

  expected_start.set_visible(true);
  expected_start.set_type(gfx::SelectionBound::LEFT);
  EXPECT_EQ(expected_start, start);

  expected_end.set_visible(true);
  expected_end.set_type(gfx::SelectionBound::RIGHT);
  EXPECT_EQ(expected_end, end);

  gfx::RectF expected_rect(
      expected_start.edge_start().x(), expected_start.edge_start().y(),
      expected_end.edge_start().x() - expected_start.edge_start().x(),
      expected_end.edge_end().y() - expected_start.edge_start().y());

  // The rect between the visible selection bounds determines the position of
  // the quick menu.
  EXPECT_EQ(expected_rect, gfx::RectFBetweenSelectionBounds(start, end));
  EXPECT_EQ(expected_rect, gfx::RectFBetweenVisibleSelectionBounds(start, end));
#endif  // BUILDFLAG(IS_MAC)
}

// When selecting something, only the copy command id should be enabled.
IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, IsCommandIdEnabledCopyEnabled) {
  EXPECT_FALSE(pdf_document_helper()->IsCommandIdEnabled(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCut),
      /*can_paste=*/true));
  EXPECT_FALSE(pdf_document_helper()->IsCommandIdEnabled(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCopy),
      /*can_paste=*/true));

  constexpr gfx::PointF kLeft(1.0f, 1.0f);
  constexpr gfx::PointF kRight(5.0f, 5.0f);
  constexpr int32_t kLeftHeight = 2;
  constexpr int32_t kRightHeight = 2;
  SelectionChanged(kLeft, kLeftHeight, kRight, kRightHeight);

  EXPECT_FALSE(pdf_document_helper()->IsCommandIdEnabled(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCut),
      /*can_paste=*/true));

#if BUILDFLAG(IS_MAC)
  // Since macOS does not support Touch Selection Editing, the copy command is
  // not enabled.
  EXPECT_FALSE(pdf_document_helper()->IsCommandIdEnabled(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCopy),
      /*can_paste=*/true));
#else
  EXPECT_TRUE(pdf_document_helper()->IsCommandIdEnabled(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCopy),
      /*can_paste=*/true));
#endif  // BUILDFLAG(IS_MAC)
}

// Test that the copy command executes.
IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, ExecuteCommandCopy) {
  base::UserActionTester action_tester;
  EXPECT_EQ(0, action_tester.GetActionCount("Copy"));

  pdf_document_helper()->ExecuteCommand(
      std::to_underlying(ui::TouchEditable::MenuCommands::kCopy), 0);

  EXPECT_EQ(1, action_tester.GetActionCount("Copy"));
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, DefaultImplementation) {
  EXPECT_FALSE(pdf_document_helper()->SupportsAnimation());
  EXPECT_FALSE(pdf_document_helper()->CreateDrawable());
  EXPECT_FALSE(pdf_document_helper()->ShouldShowQuickMenu(/*can_paste=*/true));
  EXPECT_TRUE(pdf_document_helper()->GetSelectedText().empty());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, DocumentLoadComplete) {
  base::test::TestFuture<void> load_complete_future;
  EXPECT_FALSE(pdf_document_helper()->IsDocumentLoadComplete());
  pdf_document_helper()->RegisterForDocumentLoadComplete(
      load_complete_future.GetCallback());
  pdf_document_helper()->OnDocumentLoadComplete();
  EXPECT_TRUE(load_complete_future.WaitAndClear());
  EXPECT_TRUE(pdf_document_helper()->IsDocumentLoadComplete());

  // Immediately called when document is already load complete.
  pdf_document_helper()->RegisterForDocumentLoadComplete(
      load_complete_future.GetCallback());
  EXPECT_TRUE(load_complete_future.WaitAndClear());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       HasMeaningfulTextReturnsFalseBeforeLoad) {
  base::test::TestFuture<bool> future;
  pdf_document_helper()->HasMeaningfulText(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       HasMeaningfulTextReturnsFalseWhenNoText) {
  NiceMock<FakePdfListener> listener;
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(listener, HasMeaningfulText)
      .WillOnce([](FakePdfListener::HasMeaningfulTextCallback callback) {
        std::move(callback).Run(false);
      });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<bool> future;
  pdf_document_helper()->HasMeaningfulText(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       HasMeaningfulTextReturnsTrueWhenNonEmptyText) {
  NiceMock<FakePdfListener> listener;
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(listener, HasMeaningfulText)
      .WillOnce([](FakePdfListener::HasMeaningfulTextCallback callback) {
        std::move(callback).Run(true);
      });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<bool> future;
  pdf_document_helper()->HasMeaningfulText(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       HasJavaScriptReturnsFalseBeforeLoad) {
  base::test::TestFuture<bool> future;
  pdf_document_helper()->HasJavaScript(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       HasJavaScriptReturnsTrueWhenHasJavaScript) {
  NiceMock<FakePdfListener> listener;
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(listener, HasJavaScript)
      .WillOnce([](FakePdfListener::HasJavaScriptCallback callback) {
        std::move(callback).Run(true);
      });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<bool> future;
  pdf_document_helper()->HasJavaScript(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       IsPasswordProtectedReturnsFalseBeforeLoad) {
  base::test::TestFuture<bool> future;
  pdf_document_helper()->IsPasswordProtected(future.GetCallback());
  EXPECT_FALSE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       IsPasswordProtectedReturnsTrueWhenPasswordProtected) {
  NiceMock<FakePdfListener> listener;
  mojo::Receiver<pdf::mojom::PdfListener> receiver(&listener);
  pdf_document_helper()->SetListener(receiver.BindNewPipeAndPassRemote());

  EXPECT_CALL(listener, IsPasswordProtected)
      .WillOnce([](FakePdfListener::IsPasswordProtectedCallback callback) {
        std::move(callback).Run(true);
      });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<bool> future;
  pdf_document_helper()->IsPasswordProtected(future.GetCallback());
  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       GetImageForNodeReturnsEmptyBeforeLoad) {
  base::test::TestFuture<const SkBitmap&> future;
  pdf_document_helper()->GetImageForNode(/*ax_node_id=*/42,
                                         future.GetCallback());
  EXPECT_TRUE(future.Get().drawsNothing());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest, GetImageForNodeAfterLoad) {
  NiceMock<FakePdfAccessibilityImageProvider> image_provider;
  SetAccessibilityImageProvider(image_provider);

  EXPECT_CALL(image_provider, GetImageForNode(42, testing::_))
      .WillOnce([](int32_t ax_node_id,
                   FakePdfAccessibilityImageProvider::GetImageForNodeCallback
                       callback) {
        SkBitmap bitmap;
        bitmap.allocN32Pixels(10, 20);
        std::move(callback).Run(bitmap);
      });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<const SkBitmap&> future;
  pdf_document_helper()->GetImageForNode(/*ax_node_id=*/42,
                                         future.GetCallback());
  const SkBitmap& result = future.Get();
  EXPECT_FALSE(result.drawsNothing());
  EXPECT_EQ(10, result.width());
  EXPECT_EQ(20, result.height());
}

IN_PROC_BROWSER_TEST_F(PDFDocumentHelperTest,
                       GetImageForNodeReturnsEmptyWhenDisconnected) {
  NiceMock<FakePdfAccessibilityImageProvider> image_provider;
  SetAccessibilityImageProvider(image_provider);

  EXPECT_CALL(image_provider, GetImageForNode(42, testing::_))
      .WillOnce([&image_provider](
                    int32_t ax_node_id,
                    FakePdfAccessibilityImageProvider::GetImageForNodeCallback
                        callback) { image_provider.ResetReceiver(); });

  pdf_document_helper()->OnDocumentLoadComplete();

  base::test::TestFuture<const SkBitmap&> future;
  pdf_document_helper()->GetImageForNode(/*ax_node_id=*/42,
                                         future.GetCallback());
  EXPECT_TRUE(future.Get().drawsNothing());
}

}  // namespace pdf
