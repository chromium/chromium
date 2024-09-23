// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/renderer/print_render_frame_helper.h"

#include <stddef.h>

#include <cmath>
#include <memory>
#include <string_view>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/printing/common/print.mojom-test-utils.h"
#include "components/printing/common/print.mojom.h"
#include "components/printing/common/print_params.h"
#include "components/printing/test/mock_printer.h"
#include "components/printing/test/print_test_content_renderer_client.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/test/mock_render_thread.h"
#include "content/public/test/render_view_test.h"
#include "ipc/ipc_listener.h"
#include "printing/buildflags/buildflags.h"
#include "printing/image.h"
#include "printing/mojom/print.mojom.h"
#include "printing/page_range.h"
#include "printing/print_job_constants.h"
#include "printing/printing_utils.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_range.h"
#include "third_party/blink/public/web/web_view.h"
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "components/printing/browser/print_manager_utils.h"
#include "printing/print_settings.h"
#include "printing/print_settings_conversion.h"
#endif

using blink::WebFrame;
using blink::WebLocalFrame;
using blink::WebString;

namespace printing {

namespace {

// A simple web page.
const char kHelloWorldHTML[] = "<body><p>Hello World!</p></body>";

// Web page used for testing onbeforeprint/onafterprint.
const char kBeforeAfterPrintHtml[] =
    "<body>"
    "<script>"
    "var beforePrintCount = 0;"
    "var afterPrintCount = 0;"
    "window.onbeforeprint = () => { ++beforePrintCount; };"
    "window.onafterprint = () => { ++afterPrintCount; };"
    "</script>"
    "<button id=\"print\" onclick=\"window.print();\">Hello World!</button>"
    "</body>";

// HTML with 3 pages.
const char kMultipageHTML[] =
    "<html><head><style>"
    ".break { page-break-after: always; }"
    "</style></head>"
    "<body>"
    "<div class='break'>page1</div>"
    "<div class='break'>page2</div>"
    "<div>page3</div>"
    "</body></html>";

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// A simple webpage with a button to print itself with.
const char kPrintOnUserAction[] =
    "<body>"
    "  <button id=\"print\" onclick=\"window.print();\">Hello World!</button>"
    "</body>";

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
// A simple web page with print page size css.
const char kHTMLWithPageSizeCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: 4in 4in;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A simple web page with print page layout css.
const char kHTMLWithLandscapePageCss[] =
    "<html><head><style>"
    "@media print {"
    "  @page {"
    "     size: landscape;"
    "  }"
    "}"
    "</style></head>"
    "<body>Lorem Ipsum:"
    "</body></html>";

// A longer web page.
const char kLongPageHTML[] =
    "<body><img src=\"\" width=10 height=10000 /></body>";

// A web page to simulate the print preview page.
const char kPrintPreviewHTML[] =
    "<body><p id=\"pdf-viewer\">Hello World!</p></body>";

const char kHTMLWithManyLinesOfText[] =
    "<html><head><style>"
    "p { font-size: 24px; }"
    "</style></head><body>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "<p>The quick brown fox jumped over the lazy dog.</p>"
    "</body></html>";
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class FakePrintPreviewUI : public mojom::PrintPreviewUI {
 public:
  struct PageData {
    uint32_t index;
    uint32_t content_data_size;
  };

  FakePrintPreviewUI() = default;
  ~FakePrintPreviewUI() override = default;

  mojo::PendingAssociatedRemote<mojom::PrintPreviewUI> BindReceiver() {
    return receiver_.BindNewEndpointAndPassDedicatedRemote();
  }
  void ResetPreviewStatus() {
    // Make sure there is no active request.
    DCHECK(!quit_closure_);
    preview_status_ = PreviewStatus::kNone;
  }
  // Waits until the preview request is failed, canceled, invalid, or done.
  void WaitUntilPreviewUpdate() {
    // If |preview_status_| is updated, it doesn't need to wait.
    if (preview_status_ != PreviewStatus::kNone)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }
  // Sets the page number to be cancelled.
  void set_print_preview_cancel_page_number(uint32_t page) {
    print_preview_cancel_page_number_ = page;
  }
  bool PreviewFailed() const {
    return preview_status_ == PreviewStatus::kFailed;
  }
  bool PreviewCancelled() const {
    return preview_status_ == PreviewStatus::kCancelled;
  }
  bool InvalidPrinterSetting() const {
    return preview_status_ == PreviewStatus::kInvalidSetting;
  }
  bool IsMetafileReadyForPrinting() const {
    return preview_status_ == PreviewStatus::kMetafileReadyForPrinting;
  }
  uint32_t page_count() const { return page_count_; }
  mojom::PageSizeMargins* page_layout() const {
    return page_layout_ ? page_layout_.get() : nullptr;
  }
  uint32_t print_preview_pages_remaining() const {
    return print_preview_pages_remaining_;
  }
  mojom::DidPreviewDocumentParams* did_preview_document_params() const {
    return did_preview_document_params_ ? did_preview_document_params_.get()
                                        : nullptr;
  }
  const std::vector<PageData>& print_preview_pages() const {
    return print_preview_pages_;
  }
  bool all_pages_have_custom_size() const {
    return all_pages_have_custom_size_;
  }
  bool all_pages_have_custom_orientation() const {
    return all_pages_have_custom_orientation_;
  }

  // mojom::PrintPreviewUI:
  void SetOptionsFromDocument(const mojom::OptionsFromDocumentParamsPtr params,
                              int32_t request_id) override {}
  void DidPrepareDocumentForPreview(int32_t document_cookie,
                                    int32_t request_id) override {}
  void DidPreviewPage(mojom::DidPreviewPageParamsPtr params,
                      int32_t request_id) override {
    uint32_t page_index = params->page_index;
    DCHECK_NE(page_index, kInvalidPageIndex);
    print_preview_pages_remaining_--;
    print_preview_pages_.emplace_back(
        params->page_index, params->content->metafile_data_region.GetSize());
  }
  void MetafileReadyForPrinting(mojom::DidPreviewDocumentParamsPtr params,
                                int32_t request_id) override {
    DCHECK_EQ(preview_status_, PreviewStatus::kNone);
    preview_status_ = PreviewStatus::kMetafileReadyForPrinting;
    did_preview_document_params_ = std::move(params);
    RunQuitClosure();
  }
  void PrintPreviewFailed(int32_t document_cookie,
                          int32_t request_id) override {
    DCHECK_EQ(preview_status_, PreviewStatus::kNone);
    preview_status_ = PreviewStatus::kFailed;
    RunQuitClosure();
  }
  void PrintPreviewCancelled(int32_t document_cookie,
                             int32_t request_id) override {
    DCHECK_EQ(preview_status_, PreviewStatus::kNone);
    preview_status_ = PreviewStatus::kCancelled;
    RunQuitClosure();
  }
  void PrinterSettingsInvalid(int32_t document_cookie,
                              int32_t request_id) override {
    DCHECK_EQ(preview_status_, PreviewStatus::kNone);
    preview_status_ = PreviewStatus::kInvalidSetting;
    RunQuitClosure();
  }
  void DidGetDefaultPageLayout(mojom::PageSizeMarginsPtr page_layout_in_points,
                               const gfx::RectF& printable_area_in_points,
                               bool all_pages_have_custom_size,
                               bool all_pages_have_custom_orientation,
                               int32_t request_id) override {
    page_layout_ = std::move(page_layout_in_points);
    all_pages_have_custom_size_ = all_pages_have_custom_size;
    all_pages_have_custom_orientation_ = all_pages_have_custom_orientation;
  }
  void DidStartPreview(mojom::DidStartPreviewParamsPtr params,
                       int32_t request_id) override {
    page_count_ = params->page_count;
    print_preview_pages_remaining_ = params->page_count;
  }
  // Determines whether to cancel a print preview request.
  bool ShouldCancelRequest() const {
    return print_preview_pages_remaining_ == print_preview_cancel_page_number_;
  }

 private:
  void RunQuitClosure() {
    if (!quit_closure_)
      return;
    std::move(quit_closure_).Run();
  }

  enum class PreviewStatus {
    kNone = 0,
    kFailed,
    kCancelled,
    kInvalidSetting,
    kMetafileReadyForPrinting,
  };

  PreviewStatus preview_status_ = PreviewStatus::kNone;
  uint32_t page_count_ = 0;
  bool all_pages_have_custom_size_ = false;
  bool all_pages_have_custom_orientation_ = false;
  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  uint32_t print_preview_cancel_page_number_ = kInvalidPageIndex;
  mojom::PageSizeMarginsPtr page_layout_;
  mojom::DidPreviewDocumentParamsPtr did_preview_document_params_;
  // Number of pages to generate for print preview.
  uint32_t print_preview_pages_remaining_ = 0;
  // Vector of <page_index, content_data_size> that were previewed.
  std::vector<PageData> print_preview_pages_;
  base::OnceClosure quit_closure_;
  base::OnceClosure quit_closure_for_preview_started_;

  mojo::AssociatedReceiver<mojom::PrintPreviewUI> receiver_{this};
};
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

class TestPrintManagerHost
    : public mojom::PrintManagerHostInterceptorForTesting {
 public:
  TestPrintManagerHost(content::RenderFrame* frame, MockPrinter* printer)
      : printer_(printer) {
    Init(frame);
  }
  ~TestPrintManagerHost() override = default;

  // mojom::PrintManagerHostInterceptorForTesting
  mojom::PrintManagerHost* GetForwardingInterface() override { return nullptr; }
  void DidGetPrintedPagesCount(int32_t cookie, uint32_t number_pages) override {
    if (number_pages_ > 0)
      EXPECT_EQ(number_pages, number_pages_);
    printer_->SetPrintedPagesCount(cookie, number_pages);
  }
  void DidPrintDocument(mojom::DidPrintDocumentParamsPtr params,
                        DidPrintDocumentCallback callback) override {
    base::RunLoop().RunUntilIdle();
    printer_->OnDocumentPrinted(std::move(params));
    std::move(callback).Run(true);
    is_printed_ = true;
  }
  void IsPrintingEnabled(IsPrintingEnabledCallback callback) override {
    std::move(callback).Run(is_printing_enabled_);
  }
  void GetDefaultPrintSettings(
      GetDefaultPrintSettingsCallback callback) override {
    printing::mojom::PrintParamsPtr params =
        printer_->GetDefaultPrintSettings();
    std::move(callback).Run(std::move(params));
  }
  void DidShowPrintDialog() override {}
  void ScriptedPrint(printing::mojom::ScriptedPrintParamsPtr params,
                     ScriptedPrintCallback callback) override {
    if (!print_dialog_user_response_) {
      std::move(callback).Run(nullptr);
      return;
    }

    auto settings = printing::mojom::PrintPagesParams::New();
    settings->params = printing::mojom::PrintParams::New();
    printer_->ScriptedPrint(params->cookie, params->expected_pages_count,
                            params->has_selection, settings.get());
    if (!PrintMsgPrintParamsIsValid(*settings->params)) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::move(callback).Run(std::move(settings));
  }
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void UpdatePrintSettings(base::Value::Dict job_settings,
                           UpdatePrintSettingsCallback callback) override {
    // Check and make sure the required settings are all there.
    std::optional<int> margins_type = job_settings.FindInt(kSettingMarginsType);
    if (!margins_type.has_value() ||
        !job_settings.FindBool(kSettingLandscape) ||
        !job_settings.FindBool(kSettingCollate) ||
        !job_settings.FindInt(kSettingColor) ||
        !job_settings.FindInt(kSettingPrinterType) ||
        !job_settings.FindBool(kIsFirstRequest) ||
        !job_settings.FindString(kSettingDeviceName) ||
        !job_settings.FindInt(kSettingDuplexMode) ||
        !job_settings.FindInt(kSettingCopies) ||
        !job_settings.FindInt(kPreviewUIID) ||
        !job_settings.FindInt(kPreviewRequestID)) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::unique_ptr<PrintSettings> print_settings =
        PrintSettingsFromJobSettings(job_settings);
    if (!print_settings) {
      std::move(callback).Run(nullptr);
      return;
    }

    mojom::PrintPagesParamsPtr settings = mojom::PrintPagesParams::New();
    settings->pages = GetPageRangesFromJobSettings(job_settings);
    settings->params = mojom::PrintParams::New();
    RenderParamsFromPrintSettings(*print_settings, settings->params.get());
    settings->params->document_cookie = PrintSettings::NewCookie();
    if (!PrintMsgPrintParamsIsValid(*settings->params)) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::move(callback).Run(std::move(settings));
  }
  void SetupScriptedPrintPreview(
      SetupScriptedPrintPreviewCallback callback) override {
    is_setup_scripted_print_preview_ = true;
    std::move(callback).Run();
  }
  void ShowScriptedPrintPreview(bool source_is_modifiable) override {}
  void RequestPrintPreview(
      mojom::RequestPrintPreviewParamsPtr params) override {}
  void CheckForCancel(int32_t preview_ui_id,
                      int32_t request_id,
                      CheckForCancelCallback callback) override {
    // Waits until other mojo messages are handled before checking if
    // the print preview is canceled.
    base::RunLoop().RunUntilIdle();
    std::move(callback).Run(preview_ui_->ShouldCancelRequest());
  }

  void SetAccessibilityTree(
      int32_t cookie,
      const ui::AXTreeUpdate& accessibility_tree) override {
    ++accessibility_tree_set_count_;
  }
#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

  bool IsSetupScriptedPrintPreview() {
    return is_setup_scripted_print_preview_;
  }
  void ResetSetupScriptedPrintPreview() {
    is_setup_scripted_print_preview_ = false;
  }
  bool IsPrinted() { return is_printed_; }
  void SetExpectedPagesCount(uint32_t number_pages) {
    number_pages_ = number_pages;
  }
  void WaitUntilBinding() {
    if (receiver_.is_bound())
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void SetPrintingEnabled(bool enabled) { is_printing_enabled_ = enabled; }

  // Call with |response| set to true if the user wants to print.
  // False if the user decides to cancel.
  void SetPrintDialogUserResponse(bool response) {
    print_dialog_user_response_ = response;
  }
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  void set_preview_ui(FakePrintPreviewUI* preview_ui) {
    preview_ui_ = preview_ui;
  }
#endif

  int accessibility_tree_set_count() const {
    return accessibility_tree_set_count_;
  }

 private:
  void Init(content::RenderFrame* frame) {
    frame->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::PrintManagerHost::Name_,
        base::BindRepeating(&TestPrintManagerHost::BindPrintManagerReceiver,
                            base::Unretained(this)));
  }

  void BindPrintManagerReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receiver_.Bind(mojo::PendingAssociatedReceiver<mojom::PrintManagerHost>(
        std::move(handle)));

    if (!quit_closure_)
      return;
    std::move(quit_closure_).Run();
  }

  uint32_t number_pages_ = 0;
  bool is_setup_scripted_print_preview_ = false;
  bool is_printed_ = false;
  raw_ptr<MockPrinter> printer_;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  raw_ptr<FakePrintPreviewUI> preview_ui_;
#endif
  base::OnceClosure quit_closure_;
  bool is_printing_enabled_ = true;
  // True to simulate user clicking print. False to cancel.
  bool print_dialog_user_response_ = true;
  int accessibility_tree_set_count_ = 0;
  mojo::AssociatedReceiver<mojom::PrintManagerHost> receiver_{this};
};

}  // namespace

class PrintRenderFrameHelperTestBase : public content::RenderViewTest {
 public:
  PrintRenderFrameHelperTestBase() = default;
  PrintRenderFrameHelperTestBase(const PrintRenderFrameHelperTestBase&) =
      delete;
  PrintRenderFrameHelperTestBase& operator=(
      const PrintRenderFrameHelperTestBase&) = delete;
  ~PrintRenderFrameHelperTestBase() override = default;

 protected:
  // content::RenderViewTest:
  content::ContentRendererClient* CreateContentRendererClient() override {
    return new PrintTestContentRendererClient(/*generate_tagged_pdfs=*/false);
  }

  void SetUp() override {
    render_thread_ = std::make_unique<content::MockRenderThread>();
    printer_ = std::make_unique<MockPrinter>();

    content::RenderViewTest::SetUp();

    // For `IDR_PRINT_HEADER_FOOTER_TEMPLATE_PAGE`.
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("components_tests_resources.pak"),
        ui::kScaleFactorNone);

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    preview_ui_ = std::make_unique<FakePrintPreviewUI>();
#endif
    BindPrintManagerHost(content::RenderFrame::FromWebFrame(GetMainFrame()));
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
    // Do this before shutting down V8 in RenderViewTest::TearDown().
    // http://crbug.com/328552
    __lsan_do_leak_check();
#endif

    content::RenderViewTest::TearDown();
  }

  void BindPrintManagerHost(content::RenderFrame* frame) {
    auto print_manager =
        std::make_unique<TestPrintManagerHost>(frame, printer());
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
    print_manager->set_preview_ui(preview_ui_.get());
#endif
    GetPrintRenderFrameHelperForFrame(frame)->GetPrintManagerHost();
    print_manager->WaitUntilBinding();
    frame_to_print_manager_map_.emplace(frame, std::move(print_manager));
  }

  void ClearPrintManagerHost() { frame_to_print_manager_map_.clear(); }

  void PrintWithJavaScript() {
    print_manager()->ResetSetupScriptedPrintPreview();
    ExecuteJavaScriptForTests("window.print();");
    base::RunLoop().RunUntilIdle();
  }

  // Verifies whether the pages printed or not.
  void VerifyPagesPrintedForFrame(bool expect_printed,
                                  content::RenderFrame* render_frame) {
    ASSERT_EQ(expect_printed, print_manager(render_frame)->IsPrinted());
  }

  // Same as VerifyPagesPrintedForFrame(), but defaults to the main frame.
  void VerifyPagesPrinted(bool expect_printed) {
    auto* render_frame = content::RenderFrame::FromWebFrame(GetMainFrame());
    VerifyPagesPrintedForFrame(expect_printed, render_frame);
  }

  void OnPrintPages() {
    GetPrintRenderFrameHelper()->PrintRequestedPages();
    base::RunLoop().RunUntilIdle();
  }

  void OnPrintPagesInFrame(std::string_view frame_name) {
    blink::WebFrame* frame =
        GetMainFrame()->FindFrameByName(blink::WebString::FromUTF8(frame_name));
    ASSERT_TRUE(frame);
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(frame->ToWebLocalFrame());
    BindPrintManagerHost(render_frame);
    PrintRenderFrameHelper* helper =
        GetPrintRenderFrameHelperForFrame(render_frame);
    ASSERT_TRUE(helper);
    helper->PrintRequestedPages();
    base::RunLoop().RunUntilIdle();
  }

  PrintRenderFrameHelper* GetPrintRenderFrameHelper() {
    return PrintRenderFrameHelper::Get(
        content::RenderFrame::FromWebFrame(GetMainFrame()));
  }

  PrintRenderFrameHelper* GetPrintRenderFrameHelperForFrame(
      content::RenderFrame* frame) {
    return PrintRenderFrameHelper::Get(frame);
  }

  void ClickMouseButton(const gfx::Rect& bounds) {
    EXPECT_FALSE(bounds.IsEmpty());

    blink::WebMouseEvent mouse_event(
        blink::WebInputEvent::Type::kMouseDown,
        blink::WebInputEvent::kNoModifiers,
        blink::WebInputEvent::GetStaticTimeStampForTests());
    mouse_event.button = blink::WebMouseEvent::Button::kLeft;
    mouse_event.SetPositionInWidget(bounds.CenterPoint().x(),
                                    bounds.CenterPoint().y());
    mouse_event.click_count = 1;
    SendWebMouseEvent(mouse_event);
    mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
    SendWebMouseEvent(mouse_event);
  }

  void ExpectNoBeforeNoAfterPrintEvent() {
    int result;
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"beforePrintCount", &result));
    EXPECT_EQ(0, result) << "beforeprint event should not be dispatched.";
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"afterPrintCount", &result));
    EXPECT_EQ(0, result) << "afterprint event should not be dispatched.";
  }

  void ExpectOneBeforeNoAfterPrintEvent() {
    int result;
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"beforePrintCount", &result));
    EXPECT_EQ(1, result) << "beforeprint event should be dispatched once.";
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"afterPrintCount", &result));
    EXPECT_EQ(0, result) << "afterprint event should not be dispatched.";
  }

  void ExpectOneBeforeOneAfterPrintEvent() {
    int result;
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"beforePrintCount", &result));
    EXPECT_EQ(1, result) << "beforeprint event should be dispatched once.";
    ASSERT_TRUE(
        ExecuteJavaScriptAndReturnIntValue(u"afterPrintCount", &result));
    EXPECT_EQ(1, result) << "afterprint event should be dispatched once.";
  }

  // Count the number of pixels with `target_color` in the selected area, and
  // the number of the remaining pixels that aren't white.
  struct PixelCount {
    unsigned with_target_color = 0;
    unsigned unknown_nonwhite = 0;
  };
  PixelCount CheckPixels(const Image& image,
                         uint32_t target_color,
                         const gfx::Rect& rect) {
    PixelCount count;
    for (int y = rect.y(); y < rect.bottom(); y++) {
      for (int x = rect.x(); x < rect.right(); x++) {
        uint32_t pixel = image.pixel_at(x, y);
        if (pixel == target_color) {
          count.with_target_color++;
        } else if (pixel != 0xffffffU) {
          count.unknown_nonwhite++;
        }
      }
    }
    return count;
  }

  TestPrintManagerHost* print_manager(content::RenderFrame* frame = nullptr) {
    if (!frame)
      frame = content::RenderFrame::FromWebFrame(GetMainFrame());
    auto it = frame_to_print_manager_map_.find(frame);
    return it->second.get();
  }
  MockPrinter* printer() { return printer_.get(); }
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  FakePrintPreviewUI* preview_ui() { return preview_ui_.get(); }
#endif

 private:
  // A mock printer device used for printing tests.
  std::unique_ptr<MockPrinter> printer_;
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
  std::unique_ptr<FakePrintPreviewUI> preview_ui_;
#endif
  std::map<content::RenderFrame*, std::unique_ptr<TestPrintManagerHost>>
      frame_to_print_manager_map_;
};

// RenderViewTest-based tests crash on Android
// http://crbug.com/187500
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PrintRenderFrameHelperTest DISABLED_PrintRenderFrameHelperTest
#else
#define MAYBE_PrintRenderFrameHelperTest PrintRenderFrameHelperTest
#endif  // BUILDFLAG(IS_ANDROID)

class MAYBE_PrintRenderFrameHelperTest : public PrintRenderFrameHelperTestBase {
 public:
  MAYBE_PrintRenderFrameHelperTest() = default;
  MAYBE_PrintRenderFrameHelperTest(const MAYBE_PrintRenderFrameHelperTest&) =
      delete;
  MAYBE_PrintRenderFrameHelperTest& operator=(
      const MAYBE_PrintRenderFrameHelperTest&) = delete;
  ~MAYBE_PrintRenderFrameHelperTest() override = default;
};

// This tests only for platforms without print preview.
#if !BUILDFLAG(ENABLE_PRINT_PREVIEW)
// Tests that the renderer blocks window.print() calls if they occur too
// frequently.
TEST_F(MAYBE_PrintRenderFrameHelperTest, BlockScriptInitiatedPrinting) {
  // Pretend user will cancel printing.
  print_manager()->SetPrintDialogUserResponse(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_manager()->SetPrintDialogUserResponse(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Unblock script initiated printing and verify printing works.
  GetPrintRenderFrameHelper()->scripting_throttler_.Reset();
  printer()->Reset();
  print_manager()->SetExpectedPagesCount(1);
  PrintWithJavaScript();
  VerifyPagesPrinted(true);
}

// Tests that the renderer always allows window.print() calls if they are user
// initiated.
TEST_F(MAYBE_PrintRenderFrameHelperTest, AllowUserOriginatedPrinting) {
  // Pretend user will cancel printing.
  print_manager()->SetPrintDialogUserResponse(false);
  // Try to print with window.print() a few times.
  PrintWithJavaScript();
  PrintWithJavaScript();
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Pretend user will print. (but printing is blocked.)
  print_manager()->SetPrintDialogUserResponse(true);
  PrintWithJavaScript();
  VerifyPagesPrinted(false);

  // Try again as if user initiated, without resetting the print count.
  printer()->Reset();
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  print_manager()->SetExpectedPagesCount(1);
  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);
  base::RunLoop().RunUntilIdle();

  VerifyPagesPrinted(true);
}

// Duplicate of OnPrintPagesTest only using javascript to print.
TEST_F(MAYBE_PrintRenderFrameHelperTest, PrintWithJavascript) {
  print_manager()->SetExpectedPagesCount(1);
  PrintWithJavaScript();

  VerifyPagesPrinted(true);
}

// Regression test for https://crbug.com/912966
TEST_F(MAYBE_PrintRenderFrameHelperTest, WindowPrintBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();
  print_manager()->SetExpectedPagesCount(1);

  PrintWithJavaScript();

  VerifyPagesPrinted(true);
  ExpectOneBeforeOneAfterPrintEvent();
}
#endif  // !BUILDFLAG(ENABLE_PRINT_PREVIEW)

// Tests that printing pages work and sending and receiving messages through
// that channel all works.
TEST_F(MAYBE_PrintRenderFrameHelperTest, OnPrintPages) {
  LoadHTML(kHelloWorldHTML);

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();

  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, BasicBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();

  VerifyPagesPrinted(true);
  ExpectOneBeforeOneAfterPrintEvent();
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, BasicBeforePrintAfterPrintSubFrame) {
  static const char kCloseOnBeforeHtml[] =
      "<body>Hello"
      "<iframe name=sub srcdoc='<script>"
      "window.onbeforeprint = () => { window.frameElement.remove(); };"
      "</script>'></iframe>"
      "</body>";

  LoadHTML(kCloseOnBeforeHtml);
  content::RenderFrame* sub_render_frame = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FindFrameByName("sub")->ToWebLocalFrame());
  OnPrintPagesInFrame("sub");
  EXPECT_EQ(nullptr, GetMainFrame()->FindFrameByName("sub"));
  VerifyPagesPrintedForFrame(false, sub_render_frame);

  ClearPrintManagerHost();

  static const char kCloseOnAfterHtml[] =
      "<body>Hello"
      "<iframe name=sub srcdoc='<script>"
      "window.onafterprint = () => { window.frameElement.remove(); };"
      "</script>'></iframe>"
      "</body>";

  LoadHTML(kCloseOnAfterHtml);
  sub_render_frame = content::RenderFrame::FromWebFrame(
      GetMainFrame()->FindFrameByName("sub")->ToWebLocalFrame());
  OnPrintPagesInFrame("sub");
  EXPECT_EQ(nullptr, GetMainFrame()->FindFrameByName("sub"));
  VerifyPagesPrintedForFrame(true, sub_render_frame);
}

// https://crbug.com/1372396
//
// There used to be a Blink bug when entering print preview with a monolithic
// absolutely positioned box that extended into a page where the parent had no
// representation. It could only be reproduced when entering print preview,
// because print preview apparently enters print mode, runs a rendering
// lifecycle update, leaves print mode *without* running a rendering lifecycle
// update, then enter print mode a second time. When running a rendering
// lifecycle update this time, we'd fail a DCHECK, because when leaving print
// mode the first time, we'd mark for paint invalidation. Not handling it at
// that point (no lifecycle update) is fine in principle, but it used to cause
// some bad ancestry node marking when we got to the lifecycle update when
// entering print mode for the second time.
TEST_F(MAYBE_PrintRenderFrameHelperTest, MonolithicAbsposOverflowingParent) {
  LoadHTML(R"HTML(
    <style>
      #trouble {
        contain: size;
        position: absolute;
        top: 5000px;
        width: 100px;
        height: 100px;
        background: lime;
      }
    </style>
    <div style="position:relative; height:10000px;">
      <div>
        <div id="trouble"></div>
      </div>
    </div>
  )HTML");

  OnPrintPages();
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

TEST_F(MAYBE_PrintRenderFrameHelperTest, Pixels) {
  // This test should generate two pages. The first should be 16x16 CSS pixels
  // large, and the second should be 24x24. The pixels and page size information
  // are going on a ride through the machineries, so use size values carefully,
  // to avoid subpixel issues. At some point on the journey, everything will be
  // changed to 300 DPI, and the page sizes involved will be rounded to integers
  // (so we need something that ends up with integers after having been
  // multiplied by 300/72 and back). See crbug.com/1466995 . Furthermore, the
  // final output will be in points, not CSS pixels, which is why the
  // expectation is to get 12x12 and 18x18 pages instead of 16x16 and 24x24 (and
  // a 4px border becomes a 3pt border).
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 24px;
        margin: 0;
      }
      @page:first {
        size: 16px;
      }
      body {
        margin: 0;
      }
      div {
        box-sizing: border-box;
        border: 4px solid;
      }
    </style>
    <div style="width:16px; height:16px; border-color:#00ff00;"></div>
    <div style="width:24px; height:24px; border-color:#0000ff;"></div>
  )HTML");

  printer()->set_should_generate_page_images(true);
  printer()->Params().should_print_backgrounds = true;
  OnPrintPages();

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image& first_image = page->image();
  ASSERT_EQ(first_image.size(), gfx::Size(12, 12));
  // Top left corner:
  EXPECT_EQ(first_image.pixel_at(0, 0), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(2, 2), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(3, 3), 0xffffffU);
  // Top right corner:
  EXPECT_EQ(first_image.pixel_at(11, 0), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(9, 2), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(8, 3), 0xffffffU);
  // Bottom right corner:
  EXPECT_EQ(first_image.pixel_at(11, 11), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(9, 9), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(8, 8), 0xffffffU);
  // Bottom left corner:
  EXPECT_EQ(first_image.pixel_at(0, 11), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(2, 9), 0x00ff00U);
  EXPECT_EQ(first_image.pixel_at(3, 8), 0xffffffU);

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  const Image& second_image = page->image();
  ASSERT_EQ(second_image.size(), gfx::Size(18, 18));
  // Top left corner:
  EXPECT_EQ(second_image.pixel_at(0, 0), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(2, 2), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(3, 3), 0xffffffU);
  // Top right corner:
  EXPECT_EQ(second_image.pixel_at(0, 17), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(2, 15), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(3, 14), 0xffffffU);
  // Bottom right corner:
  EXPECT_EQ(second_image.pixel_at(17, 17), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(15, 15), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(14, 14), 0xffffffU);
  // Bottom left corner:
  EXPECT_EQ(second_image.pixel_at(0, 17), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(2, 15), 0x0000ffU);
  EXPECT_EQ(second_image.pixel_at(3, 14), 0xffffffU);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, RoundingAndHeadersAndFooters) {
  // Use values that end up as fractional values. The output is converted from
  // CSS pixels (96 DPI) to points (72 DPI), and also via 300 DPI, and some
  // rounding is applied on the way. See also crbug.com/1466995
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 21px;
        margin: 4px;
      }
      body {
        margin: 0;
        background: #0000ff;
      }
    </style>
    <body></body>
  )HTML");

  // Print without headers and footers.
  printer()->set_should_generate_page_images(true);
  printer()->Params().should_print_backgrounds = true;
  printer()->Params().display_header_footer = false;
  OnPrintPages();
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  printing::Image image(page->image());

  printer()->Reset();

  // Print again, this time with headers and footers. Note that no headers or
  // footers will actually be shown, since the page margins are so small, so
  // this should look identical to the output with headers and footers turned
  // off.
  printer()->Params().display_header_footer = true;

  OnPrintPages();

  page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);

  // First check that the two results are identical.
  ASSERT_EQ(image, page->image());

  // Then check the size, and some pixels. Just check the corners. Note that
  // assumptions about how subpixels are treated are being made here, meaning
  // that if code changes cause the following expectations to fail, maybe it's
  // the test that needs to be adjusted.

  ASSERT_EQ(image.size(), gfx::Size(17, 17));

  // Top left corner:
  EXPECT_EQ(image.pixel_at(2, 2), 0xffffffU);
  EXPECT_EQ(image.pixel_at(3, 3), 0x0000ffU);

  // Top right corner:
  EXPECT_EQ(image.pixel_at(13, 2), 0xffffffU);
  EXPECT_EQ(image.pixel_at(12, 3), 0x0000ffU);

  // Bottom right corner:
  EXPECT_EQ(image.pixel_at(13, 13), 0xffffffU);
  EXPECT_EQ(image.pixel_at(12, 12), 0x0000ffU);

  // Bottom left corner:
  EXPECT_EQ(image.pixel_at(2, 13), 0xffffffU);
  EXPECT_EQ(image.pixel_at(3, 12), 0x0000ffU);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, HeaderAndFooter) {
  // The headers and footers template has a padding of 15pt. To fit something
  // that's 9pt tall, we need 24pt. Also note that all pt values used here are
  // divisble by 3, so that they convert nicely to CSS pixels (this is what the
  // layout engine uses) and back.
  const float kPageWidth = 450;
  const float kPageHeight = 450;
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 450pt;
        margin: 24pt 0;
      }
      @page second {
        margin-bottom: 21pt;
      }
      @page third {
        margin-top: 21pt;
      }
      body {
        line-height: 2em;
        background: red;
      }
    </style>
    <div>Page 1</div>
    <div style="page:second;">Page 2</div>
    <div style="page:third;">Page 3</div>
  )HTML");

  mojom::PrintParams& params = printer()->Params();
  printer()->set_should_generate_page_images(true);
  params.display_header_footer = true;
  params.should_print_backgrounds = true;
  // Use a border to draw the squares, since backgrounds are omitted for headers
  // and footers.
  params.header_template =
      u"<div class='text' "
      "style='height:9pt; border-left:9pt solid #00f;'></div>";
  params.footer_template =
      u"<div class='text' "
      "style='height:9pt; border-left:9pt solid #ff0;'></div>";

  OnPrintPages();

  // First page.
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area.
  PixelCount pixel_count =
      CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Second page.
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area.
  pixel_count = CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area. It will be missing, because
  // the margin isn't large enough.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 21, kPageWidth, 21));
  EXPECT_EQ(pixel_count.with_target_color, 0u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Third page.
  page = printer()->GetPrinterPage(2);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area. It will be missing, because
  // the margin isn't large enough.
  pixel_count = CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 21));
  EXPECT_EQ(pixel_count.with_target_color, 0u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, HeaderAndFooterFitToPrinter) {
  const float kPageWidth = 612;
  const float kPageHeight = 792;
  LoadHTML(R"HTML(
    <style>
      @page {
        /* A large square-sized page will be scaled down (and centered). Since
           the printer is in portrait mode, it means that there will be
           additional space above and below the page border-box, which in turn
           means that the requested margins can be honored. */
        size: 900pt;
        margin: 24pt 0;
      }
      @page second {
        /* When the requested page box size needs to be scaled down to fit on
           "paper", margins will also be scaled down, which may mean that
            headers and footers no longer fit. */
        size: 600pt 900pt;
        margin-top: 60pt;
      }
      @page third {
        /* The requested size is way smaller than the printer size, and the page
           is centered on "paper". Even if margins are specified as 0, there's
           still plenty of room for headers and footers. */
        size: 300px;
        margin: 0;
      }
      body {
        line-height: 2em;
        background: red;
      }
    </style>
    <div>Page 1</div>
    <div style="page:second;">Page 2</div>
    <div style="page:third;">Page 3</div>
  )HTML");

  mojom::PrintParams& params = printer()->Params();
  printer()->set_should_generate_page_images(true);
  params.display_header_footer = true;
  params.should_print_backgrounds = true;
  // Fit content to the printer size (which is US letter).
  params.print_scaling_option =
      mojom::PrintScalingOption::kCenterShrinkToFitPaper;
  params.prefer_css_page_size = false;
  // Use a border to draw the squares, since backgrounds are omitted for headers
  // and footers.
  params.header_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #00f;'></div>";
  params.footer_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #ff0;'></div>";

  OnPrintPages();

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area.
  PixelCount pixel_count =
      CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area. The margin-top on this
  // particular page is extral large, so there should be room for the header,
  // even if the margins have been scaled down along with the rest of the page
  // box.
  pixel_count = CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area. It shouldn't be there, since
  // there isn't enough room for it.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 0u);
  // Due to the margin downscaling, the red body background will intersect with
  // this area.
  EXPECT_GT(pixel_count.unknown_nonwhite, 0u);

  // Third page:
  page = printer()->GetPrinterPage(2);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the blue square in the header area. Even if the specified margins
  // on this particular page are 0, the page is small and centered on the page,
  // leaving plenty of space for headers and footers.
  pixel_count = CheckPixels(*image, 0x0000ffU, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area. Even if the specified
  // margins on this particular page are 0, the page is small and centered on
  // the page, leaving plenty of space for headers and footers.
  pixel_count = CheckPixels(*image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, FooterPartiallyOutsidePage) {
  const float kPageWidth = 150;
  const float kPageHeight = 150;
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 150pt;
        margin: 24pt 0;
      }
    </style>
    <!-- Add something wide, to trigger document downscaling. This should have
         no effect on headers and footers. -->
    <div style="width:300pt; height:10pt;"></div>
  )HTML");

  mojom::PrintParams& params = printer()->Params();
  printer()->set_should_generate_page_images(true);
  params.display_header_footer = true;
  params.footer_template =
      uR"HTML(
    <style>
      /* Lose default footer padding. */
      #footer {
        padding: 0 !important;
      }
    </style>
    <div>
      <!-- The bottom 3pt of the rectangle should be pushed off the page edge
          (and not be seen anywhere), due to the negative bottom margin, so that
          it should end up as a 9x9 square. -->
      <div style="break-inside:avoid; margin-bottom:-3pt;
                  border-left:9pt solid #ff0; height:12pt;"></div>
    </div>
    <div style="break-before:page; border-left:9pt solid #00f;
                height:9pt;"></div>
  )HTML";

  OnPrintPages();

  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image& image = page->image();
  ASSERT_EQ(image.size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the yellow square in the footer area.
  PixelCount pixel_count =
      CheckPixels(image, 0xffff00U, gfx::Rect(0, kPageHeight - 24, 9, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the blue square in the footer area.
  pixel_count = CheckPixels(image, 0x0000ffU,
                            gfx::Rect(9, kPageHeight - 24, kPageWidth - 9, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, HeaderObscuredByPageMarginBox) {
  const float kPageWidth = 150;
  const float kPageHeight = 150;
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 150pt;
        margin: 24pt 0;

        @top-right {
          width: 9pt;
          height: 9pt;
          background: #0f0;
          content: "";
        }
      }
    </style>
  )HTML");

  mojom::PrintParams& params = printer()->Params();
  printer()->set_should_generate_page_images(true);
  params.display_header_footer = true;
  params.should_print_backgrounds = true;

  // Use a border to draw the squares, since backgrounds are omitted for headers
  // and footers.
  params.header_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #f00;'></div>";
  params.footer_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #ff0;'></div>";

  OnPrintPages();

  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image& image = page->image();
  ASSERT_EQ(image.size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the green square in the header area, from the @page margin
  // box. The UA-inserted header will not be painted, since there's a @page
  // margin box in the header area.
  PixelCount pixel_count =
      CheckPixels(image, 0x00ff00U, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the yellow square in the footer area.
  pixel_count = CheckPixels(image, 0xffff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, FooterObscuredByPageMarginBox) {
  const float kPageWidth = 150;
  const float kPageHeight = 150;
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 150pt;
        margin: 24pt 0;

        @bottom-right {
          width: 9pt;
          height: 9pt;
          background: #0f0;
          content: "";
        }
      }
    </style>
  )HTML");

  mojom::PrintParams& params = printer()->Params();
  printer()->set_should_generate_page_images(true);
  params.display_header_footer = true;
  params.should_print_backgrounds = true;

  // Use a border to draw the squares, since backgrounds are omitted for headers
  // and footers.
  params.header_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #ff0;'></div>";
  params.footer_template =
      u"<div class='text' "
      "style='width:7in; height:9pt; border-left:9pt solid #f00;'></div>";

  OnPrintPages();

  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const Image& image = page->image();
  ASSERT_EQ(image.size(), gfx::Size(kPageWidth, kPageHeight));

  // Look for the yellow square in the header area.
  PixelCount pixel_count =
      CheckPixels(image, 0xffff00U, gfx::Rect(0, 0, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);

  // Look for the green square in the footer area, from the @page margin
  // box. The UA-inserted header will not be painted, since there's a @page
  // margin box in the header area.
  pixel_count = CheckPixels(image, 0x00ff00U,
                            gfx::Rect(0, kPageHeight - 24, kPageWidth, 24));
  EXPECT_EQ(pixel_count.with_target_color, 81u);
  EXPECT_EQ(pixel_count.unknown_nonwhite, 0u);
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

TEST_F(MAYBE_PrintRenderFrameHelperTest, SpecifiedPageSize1) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 400px 123px;
        margin: 0;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="width:400px; height:123px;"></div>
  )HTML");

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, SpecifiedPageSize2) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 400px 123.1px;
        margin: 0;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="width:400px; height:123.1px;"></div>
  )HTML");

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, SpecifiedPageSize3) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 400px 123.9px;
        margin: 0;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="width:400px; height:123.9px;"></div>
  )HTML");

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, MediaQueryDefaultCSSPageMargins) {
  // The default page size in these tests is US Letter (see MockPrinter). The
  // default margin is 1/2 inch on each side, and this is taken into account for
  // media query evaluation in this implementation, which is interoperable with
  // others. The spec, on the other hand, says to match against the page *box*
  // [1], not the page area [1]. I.e. margins shouldn't make any difference at
  // all, according to the spec.
  //
  // [1] https://www.w3.org/TR/css-page-3/#page-model
  LoadHTML(R"HTML(
    <style>
      @page {
        /* The default margins are overridden here (to 0) as far as page area
           size and layout are concerned, but that cannot affect media query
           evaluation, as that might cause cyclic dependencies. So the 1/2 inch
           default margins are still taken into account for media query
           evaluation.  */
        margin: 0;
      }

      /* As explained above, this media query won't match, because of the
         half-inch default margins. */
      @media (width: 8.5in) and (height: 11in) {
        div { break-before: page; }
      }
    </style>
    First page
    <div>Also first page</div>
  )HTML");

  print_manager()->SetExpectedPagesCount(1);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, MediaQueryNoCSSPageMargins) {
  LoadHTML(R"HTML(
    <style>
      @page {
        /* This has no effect on media query evaluation (it affects the page
           area size and layout, though). Only the default margins can affect
           media query evaluation. */
        margin: 100px;
      }

      /* This media query will match, since the default margins are 0,
         and the default page size is US-Letter. */
      @media (width: 8.5in) and (height: 11in) {
        div { break-before: page; }
      }
    </style>
    First page
    <div>Second page</div>
  )HTML");

  // Set the default margins to 0, and the page area size equal to the page box
  // size.
  mojom::PrintParams& params = printer()->Params();
  params.margin_left = 0;
  params.margin_top = 0;
  params.content_size = params.page_size;

  print_manager()->SetExpectedPagesCount(2);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

// See http:://crbug.com/340732144
TEST_F(MAYBE_PrintRenderFrameHelperTest, MatchMediaShrinkContent) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 500px;
        margin: 0;
      }
      body {
        margin: 0;
      }
    </style>
    <div id="shrinkme" style="height:5000px;"></div>
    <script>
      var mediaQuery = window.matchMedia("print");
      mediaQuery.addListener(function() {
        shrinkme.style.height = "2000px";
      });
    </script>
  )HTML");

  // The correct page count here should be 4 (2000px split into pages of 500px),
  // but due to crbug.com/41154234 , this doesn't work correctly (and the number
  // will be 10). For now, just make sure that we don't crash.
  OnPrintPages();
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, InputScale1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 2 shrinks this to 5 inches. Content that
  // is 50 inches tall should therefore require 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page {
        margin: 0.5in;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="height:50in;"></div>
  )HTML");

  printer()->Params().scale_factor = 2;
  print_manager()->SetExpectedPagesCount(10);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, InputScale2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 2 shrinks this to 5 inches. Content that
  // is 45.5 inches tall should therefore require just a bit more than 9 pages,
  // i.e. 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page {
        margin: 0.5in;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="height:45.5in;"></div>
  )HTML");

  printer()->Params().scale_factor = 2;
  print_manager()->SetExpectedPagesCount(10);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, InputScaleAndAvoidOverflowScale1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in and horizontal margins to 2.25in leaves
  // 4in by 10in for the page area. Setting the input scale factor to 2 shrinks
  // this to 2in by 5in. There's a 3in wide block in the test. To make it fit
  // without overflowing, Blink will increase the page area size by 3/2,
  // i.e. 50% larger, so that the final page area for layout is 3 by 7.5
  // inches. Content that is 75 inches tall should therefore require 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page {
        margin: 0.5in 2.25in;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="width:3in; height:75in;"></div>
  )HTML");

  printer()->Params().scale_factor = 2;
  print_manager()->SetExpectedPagesCount(10);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest, InputScaleAndAvoidOverflowScale2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in and horizontal margins to 2.25in leaves
  // 4in by 10in for the page area. Setting the input scale factor to 2 shrinks
  // this to 2in by 5in. There's a 3in wide block in the test. To make it fit
  // without overflowing, Blink will increase the page area size by 3/2,
  // i.e. 50% larger, so that the final page area for layout is 3 by 7.5
  // inches. Content that is 68 inches tall should therefore require just a bit
  // more than 9 pages, i.e. 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page {
        margin: 0.5in 2.25in;
      }
      body {
        margin: 0;
      }
    </style>
    <div style="width:3in; height:68in;"></div>
  )HTML");

  printer()->Params().scale_factor = 2;
  print_manager()->SetExpectedPagesCount(10);
  OnPrintPages();
  VerifyPagesPrinted(true);
}

TEST_F(MAYBE_PrintRenderFrameHelperTest,
       PrintMultiplePagesWithHeadersAndFooters) {
  LoadHTML(kMultipageHTML);

  printer()->Params().display_header_footer = true;
  print_manager()->SetExpectedPagesCount(3);
  OnPrintPages();

  VerifyPagesPrinted(true);
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

TEST_F(MAYBE_PrintRenderFrameHelperTest, PrintWithIframe) {
  // Document that populates an iframe.
  static const char html[] =
      "<html><body>Lorem Ipsum:"
      "<iframe name=\"sub1\" id=\"sub1\"></iframe><script>"
      "  document.write(frames['sub1'].name);"
      "  frames['sub1'].document.write("
      "      '<p>Cras tempus ante eu felis semper luctus!</p>');"
      "  frames['sub1'].document.close();"
      "</script></body></html>";

  printer()->set_should_generate_page_images(true);
  LoadHTML(html);

  // Find the frame and set it as the focused one.  This should mean that that
  // the printout should only contain the contents of that frame.
  WebFrame* sub1_frame =
      web_view_->MainFrame()->ToWebLocalFrame()->FindFrameByName(
          WebString::FromUTF8("sub1"));
  ASSERT_TRUE(sub1_frame);
  web_view_->SetFocusedFrame(sub1_frame);
  ASSERT_NE(web_view_->FocusedFrame(), web_view_->MainFrame());

  // Initiate printing.
  OnPrintPages();
  VerifyPagesPrinted(true);

  // Verify output through MockPrinter.
  const MockPrinter* mock_printer(printer());
  ASSERT_EQ(1, mock_printer->GetPageCount());
  const Image& image1(mock_printer->GetPrinterPage(0)->image());

  // TODO(sverrir): Figure out a way to improve this test to actually print
  // only the content of the iframe.  Currently image1 will contain the full
  // page.
  EXPECT_NE(0, image1.size().width());
  EXPECT_NE(0, image1.size().height());
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

// These print preview tests do not work on Chrome OS yet.
#if !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class PrintRenderFrameHelperPreviewTest
    : public PrintRenderFrameHelperTestBase {
 public:
  PrintRenderFrameHelperPreviewTest() = default;
  PrintRenderFrameHelperPreviewTest(const PrintRenderFrameHelperPreviewTest&) =
      delete;
  PrintRenderFrameHelperPreviewTest& operator=(
      const PrintRenderFrameHelperPreviewTest&) = delete;
  ~PrintRenderFrameHelperPreviewTest() override = default;

  void SetUp() override {
    PrintRenderFrameHelperTestBase::SetUp();
    BindToFakePrintPreviewUI();
    CreatePrintSettingsDictionary();
  }

 protected:
  void BindToFakePrintPreviewUI() {
    PrintRenderFrameHelper* frame_helper = GetPrintRenderFrameHelper();
    frame_helper->SetPrintPreviewUI(preview_ui()->BindReceiver());
  }

  void OnPrintPreview() {
    PrintRenderFrameHelper* print_render_frame_helper =
        GetPrintRenderFrameHelper();
    print_render_frame_helper->InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        mojo::NullAssociatedRemote(),
#endif
        /*has_selection=*/false);
    print_render_frame_helper->PrintPreview(print_settings_.Clone());
    preview_ui()->WaitUntilPreviewUpdate();

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)
    if (const mojom::DidPreviewDocumentParams* preview_params =
            preview_ui()->did_preview_document_params()) {
      const auto& region = preview_params->content->metafile_data_region;
      ASSERT_TRUE(region.IsValid());
      printer()->GeneratePageImages(region.Map());
    }
#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES
  }

  void OnPrintPreviewRerender() {
    preview_ui()->ResetPreviewStatus();
    GetPrintRenderFrameHelper()->PrintPreview(print_settings_.Clone());
    preview_ui()->WaitUntilPreviewUpdate();
  }

  // A function to set up the preview environment for `frame`. Done here to
  // access private members of the test class.
  void OnPrintPreviewForRenderFrame(WebLocalFrame* frame,
                                    bool has_selection,
                                    FakePrintPreviewUI* preview_ui) {
    content::RenderFrame* render_frame =
        content::RenderFrame::FromWebFrame(frame);
    BindPrintManagerHost(render_frame);
    PrintRenderFrameHelper* print_render_frame_helper =
        GetPrintRenderFrameHelperForFrame(render_frame);
    print_render_frame_helper->SetPrintPreviewUI(preview_ui->BindReceiver());
    print_render_frame_helper->InitiatePrintPreview(
#if BUILDFLAG(IS_CHROMEOS_ASH)
        mojo::NullAssociatedRemote(),
#endif
        has_selection);

    print_render_frame_helper->PrintPreview(print_settings().Clone());
    preview_ui->WaitUntilPreviewUpdate();
  }

  void OnClosePrintPreviewDialog() {
    GetPrintRenderFrameHelper()->OnPrintPreviewDialogClosed();
  }

  void OnPrintForSystemDialog() {
    GetPrintRenderFrameHelper()->PrintForSystemDialog();
  }

  void VerifyPreviewRequest(bool expect_request) {
    EXPECT_EQ(expect_request, print_manager()->IsSetupScriptedPrintPreview());
  }

  // The renderer should be done calculating the number of rendered pages
  // according to the specified settings defined in the mock render thread.
  // Verify the page count is correct.
  void VerifyPreviewPageCount(uint32_t expected_count) {
    EXPECT_EQ(expected_count, preview_ui()->page_count());
  }

  void VerifyPrintPreviewCancelled(bool expect_cancel) {
    EXPECT_EQ(expect_cancel, preview_ui()->PreviewCancelled());
  }

  void VerifyPrintPreviewFailed(bool expect_fail) {
    EXPECT_EQ(expect_fail, preview_ui()->PreviewFailed());
  }

  void VerifyPrintPreviewGenerated(bool expect_generated) {
    ASSERT_EQ(expect_generated, preview_ui()->IsMetafileReadyForPrinting());
    if (preview_ui()->IsMetafileReadyForPrinting()) {
      ASSERT_TRUE(preview_ui()->did_preview_document_params());
      const auto& param = *preview_ui()->did_preview_document_params();
      EXPECT_NE(0, param.document_cookie);
      EXPECT_NE(0U, param.expected_pages_count);

      auto mapped = param.content->metafile_data_region.Map();
      ASSERT_TRUE(mapped.IsValid());
      EXPECT_TRUE(LooksLikePdf(mapped.GetMemoryAsSpan<const uint8_t>()));
    }
  }

  void VerifyPrintPreviewInvalidPrinterSettings(bool expect_invalid_settings) {
    EXPECT_EQ(expect_invalid_settings, preview_ui()->InvalidPrinterSetting());
  }

  // `page_index` is 0-based.
  void VerifyDidPreviewPage(bool expect_generated,
                            uint32_t page_index,
                            FakePrintPreviewUI* preview_ui) {
    bool msg_found = false;
    uint32_t data_size = 0;
    for (const auto& preview : preview_ui->print_preview_pages()) {
      if (preview.index == page_index) {
        msg_found = true;
        data_size = preview.content_data_size;
        break;
      }
    }
    EXPECT_EQ(expect_generated, msg_found)
        << "For page at index " << page_index;
    if (expect_generated)
      EXPECT_NE(0U, data_size) << "For page at index " << page_index;
  }

  void VerifyDidPreviewPage(bool expect_generated, uint32_t page_index) {
    VerifyDidPreviewPage(expect_generated, page_index, preview_ui());
  }

  void VerifyDefaultPageLayout(
      int expected_content_width,
      int expected_content_height,
      int expected_margin_top,
      int expected_margin_bottom,
      int expected_margin_left,
      int expected_margin_right,
      bool expected_all_pages_have_custom_size,
      bool expected_all_pages_have_custom_orientation) {
    const mojom::PageSizeMargins* page_layout = preview_ui()->page_layout();
    ASSERT_TRUE(page_layout);
    EXPECT_EQ(expected_content_width, std::round(page_layout->content_width));
    EXPECT_EQ(expected_content_height, std::round(page_layout->content_height));
    EXPECT_EQ(expected_margin_top, std::round(page_layout->margin_top));
    EXPECT_EQ(expected_margin_bottom, std::round(page_layout->margin_bottom));
    EXPECT_EQ(expected_margin_left, std::round(page_layout->margin_left));
    EXPECT_EQ(expected_margin_right, std::round(page_layout->margin_right));
    EXPECT_EQ(expected_all_pages_have_custom_size,
              preview_ui()->all_pages_have_custom_size());
    EXPECT_EQ(expected_all_pages_have_custom_orientation,
              preview_ui()->all_pages_have_custom_orientation());
  }

  base::Value::Dict& print_settings() { return print_settings_; }

 private:
  void CreatePrintSettingsDictionary() {
    print_settings_ =
        base::Value::Dict()
            .Set(kSettingLandscape, false)
            .Set(kSettingCollate, false)
            .Set(kSettingColor, static_cast<int>(mojom::ColorModel::kGray))
            .Set(kSettingPrinterType,
                 static_cast<int>(mojom::PrinterType::kPdf))
            .Set(kSettingDuplexMode,
                 static_cast<int>(mojom::DuplexMode::kSimplex))
            .Set(kSettingCopies, 1)
            .Set(kSettingDeviceName, "dummy")
            .Set(kSettingDpiHorizontal, 72)
            .Set(kSettingDpiVertical, 72)
            .Set(kPreviewUIID, 4)
            .Set(kSettingRasterizePdf, false)
            .Set(kPreviewRequestID, 12345)
            .Set(kSettingScaleFactor, 100)
            .Set(kIsFirstRequest, true)
            .Set(kSettingMarginsType,
                 static_cast<int>(mojom::MarginType::kDefaultMargins))
            .Set(kSettingPagesPerSheet, 1)
            .Set(kSettingPreviewModifiable, true)
            .Set(kSettingPreviewIsFromArc, false)
            .Set(kSettingHeaderFooterEnabled, false)
            .Set(kSettingShouldPrintBackgrounds, false)
            .Set(kSettingShouldPrintSelectionOnly, false);

    // Using a media size with realistic dimensions for a Letter paper.
    auto media_size = base::Value::Dict()
                          .Set(kSettingMediaSizeWidthMicrons, 215900)
                          .Set(kSettingMediaSizeHeightMicrons, 279400)
                          .Set(kSettingsImageableAreaLeftMicrons, 12700)
                          .Set(kSettingsImageableAreaBottomMicrons, 0)
                          .Set(kSettingsImageableAreaRightMicrons, 209550)
                          .Set(kSettingsImageableAreaTopMicrons, 254000);
    print_settings_.Set(kSettingMediaSize, std::move(media_size));
  }

  base::Value::Dict print_settings_;
};

TEST_F(PrintRenderFrameHelperPreviewTest, BlockScriptInitiatedPrinting) {
  LoadHTML(kHelloWorldHTML);
  print_manager()->SetPrintingEnabled(false);
  PrintWithJavaScript();
  VerifyPreviewRequest(false);

  print_manager()->SetPrintingEnabled(true);
  PrintWithJavaScript();
  VerifyPreviewRequest(true);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintWithJavaScript) {
  LoadHTML(kPrintOnUserAction);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);

  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);

  VerifyPreviewRequest(true);

  OnClosePrintPreviewDialog();
}

// Tests that print preview work and sending and receiving messages through
// that channel all works.
TEST_F(PrintRenderFrameHelperPreviewTest, OnPrintPreview) {
  LoadHTML(kHelloWorldHTML);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(548, 692, 72, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewWithSrcdocSelection) {
  static const char kHTMLWithSrcdocChildFrame[] =
      "<html><body>"
      "<iframe name='srcdoc_frame' srcdoc='foo'></iframe>"
      "</body></html>";
  LoadHTML(kHTMLWithSrcdocChildFrame);

  // Create selection in the child frame.
  WebLocalFrame* srcdoc_frame =
      GetMainFrame()->FindFrameByName("srcdoc_frame")->ToWebLocalFrame();
  srcdoc_frame->ExecuteCommand("SelectAll");
  print_settings().Set(kSettingShouldPrintSelectionOnly, true);

  // Verify that print preview succeeds.

  // The subframe will need its own preview UI. Declare it here so it can be
  // passed to `VerifyDidPreviewPage` after `OnPrintPreviewForRenderFrame`
  // completes.
  std::unique_ptr<FakePrintPreviewUI> subframe_preview_ui =
      std::make_unique<FakePrintPreviewUI>();

  OnPrintPreviewForRenderFrame(srcdoc_frame, /*has_selection=*/true,
                               subframe_preview_ui.get());
  VerifyDidPreviewPage(true, 0, subframe_preview_ui.get());
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewUsesSrcdocBaseUrl) {
  GURL parent_base_url("https://example.com");
  static const char kHTMLWithSrcdocChildFrame[] =
      "<html><head><base href='%s'></head><body>"
      "<iframe name='srcdoc_frame' srcdoc='foo'></iframe>"
      "</body></html>";
  LoadHTML(base::StringPrintf(kHTMLWithSrcdocChildFrame,
                              parent_base_url.spec().c_str()));

  // Create selection in the child frame.
  WebLocalFrame* srcdoc_frame =
      GetMainFrame()->FindFrameByName("srcdoc_frame")->ToWebLocalFrame();
  srcdoc_frame->ExecuteCommand("SelectAll");
  print_settings().Set(kSettingShouldPrintSelectionOnly, true);

  // Verify that print preview succeeds.

  // The subframe will need its own preview UI. Declare it here so it can be
  // passed to `VerifyDidPreviewPage` after `OnPrintPreviewForRenderFrame`
  // completes.
  auto subframe_preview_ui = std::make_unique<FakePrintPreviewUI>();

  // Setup callback to capture the url and base_url of the intermediate preview
  // document.
  content::RenderFrame* render_frame =
      content::RenderFrame::FromWebFrame(srcdoc_frame);
  PrintRenderFrameHelper* print_render_frame_helper =
      GetPrintRenderFrameHelperForFrame(render_frame);

  GURL preview_document_url;
  GURL preview_document_base_url;
  print_render_frame_helper->SetWebDocumentCollectionCallbackForTest(
      base::BindLambdaForTesting([&](const blink::WebDocument& document) {
        preview_document_url = document.Url();
        preview_document_base_url = document.BaseURL();
      }));

  // Do the print preview.
  OnPrintPreviewForRenderFrame(srcdoc_frame, /*has_selection=*/true,
                               subframe_preview_ui.get());

  VerifyDidPreviewPage(true, 0, subframe_preview_ui.get());
  EXPECT_EQ(GURL(url::kAboutBlankURL), preview_document_url);
  EXPECT_EQ(parent_base_url, preview_document_base_url);
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewHTMLWithPageMarginsCss) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageMarginsCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageMarginsCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(518, 432, 216, 144, 22, 72, false, false);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview ignores print media css when non-default
// margin is selected.
TEST_F(PrintRenderFrameHelperPreviewTest,
       NonDefaultMarginsSelectedIgnorePrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kNoMargins));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(612, 792, 0, 0, 0, 0, true, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview honor print media size css when
// PRINT_TO_PDF is selected and doesn't fit to printer default paper size.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintToPDFSelectedHonorPrintCss) {
  LoadHTML(kHTMLWithPageSizeCss);

  print_settings().Set(
      kSettingMarginsType,
      static_cast<int>(mojom::MarginType::kPrintableAreaMargins));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(234, 216, 72, 0, 36, 18, true, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PreviewLayoutTriggeredByResize) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<!DOCTYPE html>"
      "<style>"
      "@media (min-width: 540px) {"
      "  #container {"
      "    width: 540px;"
      "  }"
      "}"
      ".testlist {"
      "  list-style-type: none;"
      "}"
      "</style>"
      "<div id='container'>"
      "  <ul class='testlist'>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm':"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "    <li>"
      "      <p>"
      "      'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "      bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
      "      ccccccccccccccccccccccccccccccccccccccc"
      "      ddddddddddddddddddddddddddddddddddddddd"
      "      eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"
      "      fffffffffffffffffffffffffffffffffffffff"
      "      ggggggggggggggggggggggggggggggggggggggg"
      "      hhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhhh"
      "      iiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiii"
      "      jjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjjj"
      "      kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk"
      "      lllllllllllllllllllllllllllllllllllllll"
      "      mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm'"
      "      </p>"
      "    </li>"
      "  </ul>"
      "</div>";
  LoadHTML(kHTMLWithPageCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyPreviewPageCount(2);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview honor print margin css when PRINT_TO_PDF
// is selected and doesn't fit to printer default paper size.
TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintToPDFSelectedHonorPageMarginsCss) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     margin: 3in 1in 2in 0.3in;"
      "     size: 14in 14in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  // Since PRINT_TO_PDF is selected, pdf page size is equal to print media page
  // size.
  VerifyDefaultPageLayout(914, 648, 216, 144, 22, 72, true, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow center the html page contents to
// fit the page size.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewCenterToFitPage) {
  LoadHTML(kHTMLWithPageSizeCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(224, 188, 324, 280, 198, 190, true, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow scale the html page contents to
// fit the page size.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewShrinkToFitPage) {
  // A simple web page with print margins css.
  static const char kHTMLWithPageCss[] =
      "<html><head><style>"
      "@media print {"
      "  @page {"
      "     size: 15in 17in;"
      "  }"
      "}"
      "</style></head>"
      "<body>Lorem Ipsum:"
      "</body></html>";
  LoadHTML(kHTMLWithPageCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 637, 90, 65, 20, 16, true, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow scale the html page contents to
// fit the page size, and that orientation implied by specified CSS page size is
// honored, even though the size itself is to be ignored.
TEST_F(PrintRenderFrameHelperPreviewTest, ShrinkToFitPageMatchOrientation) {
  LoadHTML(R"HTML(
      <style>
        @page { size: 17in 15in; }
      </style>
      :-D
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  base::Value::Dict custom_margins;
  custom_margins.Set(kSettingMarginTop, 10);
  custom_margins.Set(kSettingMarginRight, 20);
  custom_margins.Set(kSettingMarginBottom, 30);
  custom_margins.Set(kSettingMarginLeft, 40);
  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kCustomMargins));
  print_settings().Set(kSettingMarginsCustom, std::move(custom_margins));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(732, 572, 10, 30, 40, 20, true, true);
  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow scale the html page contents to
// fit the page size, and that orientation implied by specified CSS page size is
// honored.
TEST_F(PrintRenderFrameHelperPreviewTest,
       ShrinkToFitPageMatchOrientationCssMargins) {
  LoadHTML(R"HTML(
      <style>
        @page {
          size: 20in 17in;
          margin: 1in 2in 3in 4in;
        }
      </style>
      :-D
  )HTML");
  // The default page size is 8.5 by 11 inches. The @page descriptor wants it in
  // landscape mode, so 11 by 8.5 inches, then. The content should be scaled to
  // fit on the page. The requested page size is 20 by 17 inches. Figure out
  // which axis needs the most scaling. 20/11 < 17/8.5. 17/8.5 is 2. The content
  // needs to be scaled down by a factor of 2. To retain the aspect ratio of the
  // paper size, additional horizontal margins will be inserted, so that the
  // page width before scaling becomes 22in (11*2). The requested page size is
  // 20in, so add an additional 1in to the left and the right margins. This
  // means that the result would be the same as if this were in the CSS:
  //
  // @page {
  //   size: 22in 17in;
  //   margin: 1in 3in 3in 5in;
  // }
  //
  // Then scale everything down by a factor of 2.

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(504, 468, 36, 108, 180, 108, true, true);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsAndInputScaleToPdf1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 200% shrinks this to 5 inches. Content
  // that is 50 inches tall should therefore require 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; }
      body { margin:0; }
    </style>
    <div style="height:50in;"></div>
  )HTML");

  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false, false);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsAndInputScaleToPdf2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 200% shrinks this to 5 inches. Content
  // that is 45.5 inches tall should therefore require just a bit more than 9
  // pages, i.e. 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; }
      body { margin:0; }
    </style>
    <div style="height:45.5in;"></div>
  )HTML");

  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false, false);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsAndInputScaleToPrinter1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 200% shrinks this to 5 inches. Content
  // that is 50 inches tall should therefore require 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; }
      body { margin:0; }
    </style>
    <div style="height:50in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false, false);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsAndInputScaleToPrinter2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting vertical margins to 0.5in results in a page area of 10 inches.
  // Setting the input scale factor to 200% shrinks this to 5 inches. Content
  // that is 45.5 inches tall should therefore require just a bit more than 9
  // pages, i.e. 10 pages.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; }
      body { margin:0; }
    </style>
    <div style="height:45.5in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(540, 720, 36, 36, 36, 36, false, false);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsSizeAndInputScaleToPrinter1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page width to 34 inches and vertical margins to 1 inch results
  // in a page area of 32 inches. Setting the input scale factor to 200% shrinks
  // this to 16 inches. Content that is 160 inches tall should therefore require
  // 10 pages. Furthermore, setting the page width to 34 inches and having to
  // fit this to the actual "paper" means that everything needs to be scaled
  // down by 34/8.5 = 4. This also applies to the final margins. Horizontal
  // margins will therefore become 1/4 inch. Being in portrait mode, the actual
  // "paper" height is larger than the width, although the CSS-specified page
  // size has the same height and width. In order to resolve the
  // over-constrained situation, this means that vertical margins will be
  // adjusted to center the page area on "paper".
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:34in; }
      body { margin:0; }
    </style>
    <div style="height:160in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 576, 108, 108, 18, 18, true, true);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, MarginsSizeAndInputScaleToPrinter2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page width to 34 inches and vertical margins to 1 inch results
  // in a page area of 32 inches. Setting the input scale factor to 200% shrinks
  // this to 16 inches. Content that is 145 inches tall should therefore require
  // just a bit more than 9 pages, i.e. 10 pages. Furthermore, setting the page
  // width to 34 inches and having to fit this to the actual "paper" means that
  // everything needs to be scaled down by 34/8.5 = 4. This also applies to the
  // final margins. Horizontal margins will therefore become 1/4 inch. Being in
  // portrait mode, the actual "paper" height is larger than the width, although
  // the CSS-specified page size has the same height and width. In order to
  // resolve the over-constrained situation, this means that vertical margins
  // will be adjusted to center the page area on "paper".
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:34in; }
      body { margin:0; }
    </style>
    <div style="height:145in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 576, 108, 108, 18, 18, true, true);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter1) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page width to 34 inches and vertical margins to 1 inch results
  // in a page area of 32 inches. Setting the input scale factor to 200% shrinks
  // this to 16 inches. There's a 48in wide block in the test. To make it fit
  // without overflowing, Blink will increase the page area size by 3/2 (48/32),
  // i.e. 50% larger, so that the final page area for layout is 24 by 24 inches.
  // Content that is 240 inches tall should therefore require 10
  // pages. Furthermore, setting the page width to 34 inches and having to fit
  // this to the actual "paper" means that everything needs to be scaled down by
  // 34/8.5 = 4. This also applies to the final margins. Horizontal margins will
  // therefore become 1/4 inch. Being in portrait mode, the actual "paper"
  // height is larger than the width, although the CSS-specified page size has
  // the same height and width. In order to resolve the over-constrained
  // situation, this means that vertical margins will be adjusted to center the
  // page area on "paper".
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:34in; }
      body { margin:0; }
    </style>
    <div style="width:48in; height:240in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 576, 108, 108, 18, 18, true, true);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter2) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page width to 34 inches and vertical margins to 1 inch results
  // in a page area of 32 inches. Setting the input scale factor to 200% shrinks
  // this to 16 inches. There's a 48in wide block in the test. To make it fit
  // without overflowing, Blink will increase the page area size by 3/2 (48/32),
  // i.e. 50% larger, so that the final page area for layout is 24 by 24 inches.
  // Content that is 217 inches tall should therefore require just a bit more
  // than 9 pages, i.e. 10 pages. Furthermore, setting the page width to 34
  // inches and having to fit this to the actual "paper" means that everything
  // needs to be scaled down by 34/8.5 = 4. This also applies to the final
  // margins. Horizontal margins will therefore become 1/4 inch. Being in
  // portrait mode, the actual "paper" height is larger than the width, although
  // the CSS-specified page size has the same height and width. In order to
  // resolve the over-constrained situation, this means that vertical margins
  // will be adjusted to center the page area on "paper".
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:34in; }
      body { margin:0; }
    </style>
    <div style="width:48in; height:217in;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 576, 108, 108, 18, 18, true, true);
  VerifyPreviewPageCount(10);
  OnClosePrintPreviewDialog();
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter3) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page size to 3 inches and margins to 0.5 inches results in a
  // page area of 2 by 2 inches. Setting the input scale factor to 200% shrinks
  // this to 1 inch. There's a 1.5in wide block in the test. To make it fit
  // without overflowing, Blink will increase the page area size by 3/2 (1.5/1),
  // i.e. 50% larger, so that the final page area for layout is 1.5 by 1.5
  // inches. Content that is 5.25 inches tall should therefore require 3 and a
  // half pages (1.5 * 3.5 = 5.25). The specified page size is smaller than the
  // paper (8.5x11 inches), and should be centered on paper, meaning that the
  // margins will be changed so that the sum of the margins and the page size
  // will be equal to the paper size.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; size:3in; }
      body { margin:0; }
    </style>
    <div style="width:1.5in; height:5.25in; background:#0000ff;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(144, 144, 324, 324, 234, 234, true, true);
  VerifyPreviewPageCount(4);

  // Look for the #0000ff background on all the pages.

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(233, 323), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(234, 324), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(377, 467), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(378, 468), 0xffffffU);  // white outside

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(233, 323), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(234, 324), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(377, 467), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(378, 468), 0xffffffU);  // white outside

  // Third page:
  page = printer()->GetPrinterPage(2);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(233, 323), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(234, 324), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(377, 467), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(378, 468), 0xffffffU);  // white outside

  // Fourth and last page:
  page = printer()->GetPrinterPage(3);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(233, 323), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(234, 324), 0x0000ffU);  // blue inside
  // Bottom right corner of the DIV (which occupies half of the last page).
  EXPECT_EQ(image->pixel_at(377, 395), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(378, 396), 0xffffffU);  // white outside

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter4) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page size to 5 by 3 inches and margins to 0.5 inches results in
  // a page area of 4 by 2 inches. Setting the input scale factor to 200%
  // shrinks this to 2 by 1 inches. There's a 3in wide block in the test. To
  // make it fit without overflowing, Blink will increase the page area size by
  // 3/2, i.e. 50% larger, so that the final page area for layout is 3 by 1.5
  // inches. Content that is 2.25 inches tall should therefore require one and a
  // half page (1.5 * 1.5 = 2.25). The specified page size is smaller than the
  // paper (8.5x11 inches), and should be centered on paper, meaning that the
  // margins will be changed so that the sum of the margins and the page size
  // will be equal to the paper size. Additionally, the orientation is
  // landscape.
  LoadHTML(R"HTML(
    <style>
      @page { margin:0.5in; size:5in 3in; }
      body { margin:0; }
    </style>
    <div style="width:3in; height:2.25in; background:#0000ff;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(288, 144, 234, 234, 252, 252, true, true);
  VerifyPreviewPageCount(2);

  // Look for the #0000ff background on all the pages.

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(792, 612));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(251, 233), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(252, 234), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(539, 377), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(540, 378), 0xffffffU);  // white outside

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(792, 612));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(251, 233), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(252, 234), 0x0000ffU);  // blue inside
  // Bottom right corner of the DIV (which occupies half of the last page).
  EXPECT_EQ(image->pixel_at(539, 305), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(540, 306), 0xffffffU);  // white outside

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter5) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page size to 34 inches and margins to 1 inch results in a page
  // area of 32 inches. Setting the input scale factor to 200% shrinks this to
  // 16 inches. There's a 24in wide block in the test. To make it fit without
  // overflowing, Blink will increase the page area size by 3/2 (24/16),
  // i.e. 50% larger, so that the final page area for layout is 24 by 24 inches.
  // Content that is 36 inches tall should therefore require one and a half page
  // (1.5 * 1.5 = 2.25). The specified page size is larger than the paper
  // (8.5x11 inches), and needs to be zoomed down to fit. The zoom factor will
  // be 0.25. 8.5 / 34 (short edges) = 0.25, which is less than 11 / 34 (long
  // edges). Margins are also adjusted accordingly, since it's essentially the
  // entire page box that's scaled down. The result should be centered on paper.
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:34in; }
      body { margin:0; }
    </style>
    <div style="width:24in; height:36in; background:#0000ff;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(576, 576, 108, 108, 18, 18, true, true);
  VerifyPreviewPageCount(2);

  // Look for the #0000ff background on the first and last pages.

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(17, 107), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(18, 108), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(557, 683), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(558, 684), 0xffffffU);  // white outside

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(612, 792));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(17, 107), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(18, 108), 0x0000ffU);  // blue inside
  // Bottom right corner of the DIV (which occupies half of the last page).
  EXPECT_EQ(image->pixel_at(557, 395), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(558, 396), 0xffffffU);  // white outside

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       MarginsSizeAndInputScaleAndAvoidOverflowScaleToPrinter6) {
  // The default page size in these tests is US Letter - 8.5 by 11 inches.
  // Setting the page size to 33 by 18 inches and margins to 1 inch results in a
  // page area of 31 by 16 inches. Setting the input scale factor to 200%
  // shrinks this to 15.5 by 8 inches. There's a 23.25in wide block in the
  // test. To make it fit without overflowing, Blink will increase the page area
  // size by 3/2 (23.25/15.5), i.e. 50% larger, so that the final page area for
  // layout is 23.25 by 12 inches. Content that is 18 inches tall should
  // therefore require one and a half page (12 * 1.5 = 18). The specified page
  // size is larger than the paper (8.5x11 inches), and needs to be zoomed down
  // to fit. The zoom factor will be 1/3. 11 / 33 (long edges) = 1/3, which is
  // less than 8.5 / 18 (short edges). Margins are also adjusted accordingly,
  // since it's essentially the entire page box that's scaled down.
  // Additionally, the orientation is landscape. The result should be centered
  // on paper.
  LoadHTML(R"HTML(
    <style>
      @page { margin:1in; size:33in 18in; }
      body { margin:0; }
    </style>
    <div style="width:23.25in; height:18in; background:#0000ff;"></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingScaleFactor, 200);
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(744, 384, 114, 114, 24, 24, true, true);
  VerifyPreviewPageCount(2);

  // Look for the #0000ff background on the first and last pages.

  // First page:
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image* image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(792, 612));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(23, 113), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(24, 114), 0x0000ffU);  // blue inside
  // Bottom right page content area corner.
  EXPECT_EQ(image->pixel_at(767, 497), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(768, 498), 0xffffffU);  // white outside

  // Second page:
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  image = &page->image();
  ASSERT_EQ(image->size(), gfx::Size(792, 612));
  // Top left page content area corner.
  EXPECT_EQ(image->pixel_at(23, 113), 0xffffffU);  // white outside
  EXPECT_EQ(image->pixel_at(24, 114), 0x0000ffU);  // blue inside
  // Bottom right corner of the DIV (which occupies half of the last page).
  EXPECT_EQ(image->pixel_at(767, 305), 0x0000ffU);  // blue inside
  EXPECT_EQ(image->pixel_at(768, 306), 0xffffffU);  // white outside

  OnClosePrintPreviewDialog();
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

// Test to verify that print preview workflow honor the orientation settings
// specified in css.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewHonorsOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kNoMargins));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(792, 612, 0, 0, 0, 0, false, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that print preview workflow honors the orientation settings
// specified in css when PRINT_TO_PDF is selected.
TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintToPDFSelectedHonorOrientationCss) {
  LoadHTML(kHTMLWithLandscapePageCss);

  base::Value::Dict custom_margins;
  custom_margins.Set(kSettingMarginTop, 21);
  custom_margins.Set(kSettingMarginBottom, 23);
  custom_margins.Set(kSettingMarginLeft, 21);
  custom_margins.Set(kSettingMarginRight, 23);

  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kCustomMargins));
  print_settings().Set(kSettingMarginsCustom, std::move(custom_margins));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDefaultPageLayout(748, 568, 21, 23, 21, 23, false, true);
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewForMultiplePages) {
  LoadHTML(kMultipageHTML);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyDidPreviewPage(true, 2);
  VerifyPreviewPageCount(3);
  VerifyDefaultPageLayout(548, 692, 72, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintPreviewForMultiplePagesWithHeadersAndFooters) {
  LoadHTML(kMultipageHTML);

  print_settings().Set(kSettingHeaderFooterEnabled, true);
  print_settings().Set(kSettingHeaderFooterTitle, "The Chromiums");
  print_settings().Set(kSettingHeaderFooterURL, "https://chromium.org");
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyDidPreviewPage(true, 2);
  VerifyPreviewPageCount(3);
  VerifyDefaultPageLayout(548, 678, 86, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedPages) {
  LoadHTML(kMultipageHTML);

  // Set a page range and update the dictionary to generate only the complete
  // metafile with the selected pages. Page numbers used in the dictionary
  // are 1-based.
  base::Value::Dict page_range;
  page_range.Set(kSettingPageRangeFrom, base::Value(2));
  page_range.Set(kSettingPageRangeTo, base::Value(3));
  base::Value::List page_range_array;
  page_range_array.Append(std::move(page_range));
  print_settings().Set(kSettingPageRange, std::move(page_range_array));

  OnPrintPreview();

  // The expected page count below is 3 because the total number of pages in the
  // document, without the page range, is 3. Since only 2 pages have been
  // generated, the print_preview_pages_remaining() result is 1.
  // TODO(thestig): Fix this on the browser side to accept the number of actual
  // pages generated instead, or to take both page counts.
  EXPECT_EQ(1u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(false, 0);
  VerifyDidPreviewPage(true, 1);
  VerifyDidPreviewPage(true, 2);
  VerifyPreviewPageCount(3);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewInvalidPageRange) {
  LoadHTML(kHelloWorldHTML);

  // Request a page beyond the end of document and assure we get the entire
  // document back.
  base::Value::Dict page_range;
  page_range.Set(kSettingPageRangeFrom, 2);
  page_range.Set(kSettingPageRangeTo, 2);
  base::Value::List page_range_array;
  page_range_array.Append(std::move(page_range));
  print_settings().Set(kSettingPageRange, std::move(page_range_array));

  OnPrintPreview();

  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  OnClosePrintPreviewDialog();
}

// Test to verify that preview generated only for one page.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedText) {
  LoadHTML(kMultipageHTML);
  GetMainFrame()->SelectRange(blink::WebRange(1, 3),
                              blink::WebLocalFrame::kHideSelectionHandle,
                              blink::mojom::SelectionMenuBehavior::kHide,
                              blink::WebLocalFrame::kSelectionSetFocus);

  print_settings().Set(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Test to verify that preview generated only for two pages.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewForSelectedText2) {
  LoadHTML(kMultipageHTML);
  GetMainFrame()->SelectRange(blink::WebRange(1, 8),
                              blink::WebLocalFrame::kHideSelectionHandle,
                              blink::mojom::SelectionMenuBehavior::kHide,
                              blink::WebLocalFrame::kSelectionSetFocus);

  print_settings().Set(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(2);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewForManyLinesOfText) {
  LoadHTML(kHTMLWithManyLinesOfText);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintPreviewForManyLinesOfTextWithScaling) {
  LoadHTML(kHTMLWithManyLinesOfText);

  print_settings().Set(kSettingScaleFactor, 200);

  OnPrintPreview();

  constexpr int kExpectedPageCount = 3;
  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  for (int i = 0; i < kExpectedPageCount; ++i)
    VerifyDidPreviewPage(true, i);
  VerifyPreviewPageCount(kExpectedPageCount);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintPreviewForManyLinesOfTextWithTextSelection) {
  LoadHTML(kHTMLWithManyLinesOfText);
  GetMainFrame()->ExecuteCommand("SelectAll");

  print_settings().Set(kSettingShouldPrintSelectionOnly, true);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       PrintPreviewForManyLinesOfTextWithTextSelectionAndScaling) {
  LoadHTML(kHTMLWithManyLinesOfText);
  GetMainFrame()->ExecuteCommand("SelectAll");

  print_settings().Set(kSettingShouldPrintSelectionOnly, true);
  print_settings().Set(kSettingScaleFactor, 200);

  OnPrintPreview();

  constexpr int kExpectedPageCount = 3;
  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  for (int i = 0; i < kExpectedPageCount; ++i)
    VerifyDidPreviewPage(true, i);
  VerifyPreviewPageCount(kExpectedPageCount);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

TEST_F(PrintRenderFrameHelperPreviewTest, TextSelectionPageRules) {
  LoadHTML(R"HTML(
    <style>
      @page :first {
        size: 300px;
        page-orientation: rotate-right;
      }
      @page {
        size: 400px;
        page-orientation: rotate-left;
      }
    </style>
    <div style="break-after:page;">page 1</div>
    <div style="break-after:page;">page 2</div>
  )HTML");
  GetMainFrame()->ExecuteCommand("SelectAll");

  print_settings().Set(kSettingShouldPrintSelectionOnly, true);
  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  VerifyPreviewPageCount(2);

  // The @page rules should be ignored when printing the selection. The default
  // page size (US Letter in this case) should be used.
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  EXPECT_EQ(page->image().size(), gfx::Size(612, 792));
  page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  EXPECT_EQ(page->image().size(), gfx::Size(612, 792));

  OnClosePrintPreviewDialog();
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

// Tests that cancelling print preview works.
TEST_F(PrintRenderFrameHelperPreviewTest, PrintPreviewCancel) {
  LoadHTML(kLongPageHTML);

  const uint32_t kCancelPage = 3;
  preview_ui()->set_print_preview_cancel_page_number(kCancelPage);

  OnPrintPreview();

  EXPECT_EQ(kCancelPage, preview_ui()->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(true);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);
  VerifyPagesPrinted(false);

  OnClosePrintPreviewDialog();
}

// Tests that when default printer has invalid printer settings, print preview
// receives error message.
TEST_F(PrintRenderFrameHelperPreviewTest,
       OnPrintPreviewUsingInvalidPrinterSettings) {
  LoadHTML(kPrintPreviewHTML);

  // Set mock printer to provide invalid settings.
  // A missing color entry is invalid.
  print_settings().Remove(kSettingColor);

  OnPrintPreview();

  // We should have received invalid printer settings from |printer_|.
  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);

  OnClosePrintPreviewDialog();
}

// Tests that when the selected printer has an invalid media size, print preview
// receives error message.
TEST_F(PrintRenderFrameHelperPreviewTest, OnPrintPreviewUsingInvalidMediaSize) {
  LoadHTML(kPrintPreviewHTML);

  print_settings().Set(kSettingMediaSize, base::Value::Dict());

  OnPrintPreview();

  VerifyPrintPreviewInvalidPrinterSettings(true);
  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());

  // It should receive the invalid printer settings message only.
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(false);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, BasicBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  ExpectNoBeforeNoAfterPrintEvent();

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(548, 692, 72, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
  ExpectOneBeforeNoAfterPrintEvent();

  OnClosePrintPreviewDialog();
  ExpectOneBeforeOneAfterPrintEvent();
}

// Regression test for https://crbug.com/912966
TEST_F(PrintRenderFrameHelperPreviewTest, WindowPrintBeforePrintAfterPrint) {
  LoadHTML(kBeforeAfterPrintHtml);
  gfx::Size new_size(200, 100);
  Resize(new_size, false);
  ExpectNoBeforeNoAfterPrintEvent();

  gfx::Rect bounds = GetElementBounds("print");
  ClickMouseButton(bounds);

  VerifyPreviewRequest(true);
  ExpectOneBeforeNoAfterPrintEvent();

  OnClosePrintPreviewDialog();
  ExpectOneBeforeOneAfterPrintEvent();
}

TEST_F(PrintRenderFrameHelperPreviewTest, OnlySomePagesWithCustomSize) {
  // A specified page size will set both size and orientation (not just for any
  // given fixed size, but also for well-known page sizes, such as B5). In this
  // test, however, only the first page will match the @page rule, whereas the
  // size and orientation of the second page may be freely controlled by UI
  // settings.
  LoadHTML(R"HTML(
    <style>
      @page custom { size:B5; }
    </style>
    <div style="page:custom;">Custom page</div>
    Default page
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  EXPECT_FALSE(preview_ui()->all_pages_have_custom_size());
  EXPECT_FALSE(preview_ui()->all_pages_have_custom_orientation());
  VerifyPreviewPageCount(2);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, SingleNamedPageWithCustomSize) {
  // There's only one page, and that page is named, and it matches the @page
  // rule, which specifies a page size. There are no pages that can be
  // controlled by UI options, so the options should be hidden.
  LoadHTML(R"HTML(
    <style>
      @page custom { size:B5; }
    </style>
    <div style="page:custom;">Custom page</div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  EXPECT_TRUE(preview_ui()->all_pages_have_custom_size());
  EXPECT_TRUE(preview_ui()->all_pages_have_custom_orientation());
  VerifyPreviewPageCount(1);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, OnlySomePagesWithCustomOrientation) {
  LoadHTML(R"HTML(
    <style>
      @page custom { size:portrait; }
    </style>
    <div style="page:custom;">Custom page</div>
    Default page
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  EXPECT_FALSE(preview_ui()->all_pages_have_custom_size());
  EXPECT_FALSE(preview_ui()->all_pages_have_custom_orientation());
  VerifyPreviewPageCount(2);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       SingleNamedPageWithCustomOrientation) {
  LoadHTML(R"HTML(
    <style>
      @page custom { size:portrait; }
    </style>
    <div style="page:custom;">Custom page</div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  EXPECT_FALSE(preview_ui()->all_pages_have_custom_size());
  EXPECT_TRUE(preview_ui()->all_pages_have_custom_orientation());
  VerifyPreviewPageCount(1);

  OnClosePrintPreviewDialog();
}

TEST_F(PrintRenderFrameHelperPreviewTest, PrintForSystemDialog) {
  LoadHTML(kHelloWorldHTML);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);
  VerifyPreviewPageCount(1);

  // No need to call OnClosePrintPreviewDialog(), as OnPrintForSystemDialog()
  // takes care of the Print Preview to system print dialog transition.
  OnPrintForSystemDialog();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(true);
}

#if defined(MOCK_PRINTER_SUPPORTS_PAGE_IMAGES)

TEST_F(PrintRenderFrameHelperPreviewTest, IgnorePageSizeAndMargin) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: 2000px;
        margin: 100px;
      }
      html, body { height:100%; }
      body {
        margin: 0;
      }
      .flex {
        display: flex;
        height: 100%;
        justify-content: flex-end;
        align-items: flex-end;
      }
      .flex > div {
        width: 1pt;
        height: 1pt;
        background: #00ff00;
      }
    </style>
    <div class="flex"><div></div></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  base::Value::Dict custom_margins;
  custom_margins.Set(kSettingMarginTop, 12);
  custom_margins.Set(kSettingMarginRight, 6);
  custom_margins.Set(kSettingMarginBottom, 12);
  custom_margins.Set(kSettingMarginLeft, 6);
  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kCustomMargins));
  print_settings().Set(kSettingMarginsCustom, std::move(custom_margins));

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();

  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image& image(page->image());

  // The specified page size is much larger than 8.5x11 inches, but it should be
  // ignored, because CSS page size and margins are to be ignored, according to
  // the settings above.

  ASSERT_EQ(image.size(), gfx::Size(612, 792));

  // Find the green point in the bottom right corner of the page.
  EXPECT_EQ(image.pixel_at(605, 779), 0x00ff00U);
}

TEST_F(PrintRenderFrameHelperPreviewTest, LandscapeIgnorePageSizeAndMargin) {
  LoadHTML(R"HTML(
    <style>
      @page {
        size: landscape;
        margin: 100px;
      }
      html, body { height:100%; }
      body {
        margin: 0;
      }
      .flex {
        display: flex;
        height: 100%;
        justify-content: flex-end;
        align-items: flex-end;
      }
      .flex > div {
        width: 1pt;
        height: 1pt;
        background: #00ff00;
      }
    </style>
    <div class="flex"><div></div></div>
  )HTML");

  print_settings().Set(kSettingPrinterType,
                       static_cast<int>(mojom::PrinterType::kLocal));
  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  base::Value::Dict custom_margins;
  // TODO(crbug.com/40280219): Would be neat to test with different vertical and
  // horizontal margins here.
  custom_margins.Set(kSettingMarginTop, 12);
  custom_margins.Set(kSettingMarginRight, 12);
  custom_margins.Set(kSettingMarginBottom, 12);
  custom_margins.Set(kSettingMarginLeft, 12);
  print_settings().Set(kSettingMarginsType,
                       static_cast<int>(mojom::MarginType::kCustomMargins));
  print_settings().Set(kSettingMarginsCustom, std::move(custom_margins));

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();
  const MockPrinterPage* page = printer()->GetPrinterPage(0);
  ASSERT_TRUE(page);
  const printing::Image& image(page->image());

  // Specified page size should be ignored, according to the settings
  // above. Page orientation (landscape vs portrait) should still be honored,
  // though.

  ASSERT_EQ(image.size(), gfx::Size(792, 612));

  // Find the green point in the bottom right corner of the page.
  EXPECT_EQ(image.pixel_at(779, 599), 0x00ff00U);
}

TEST_F(PrintRenderFrameHelperPreviewTest,
       NonDefaultFirstPageSizeDefaultSecond) {
  LoadHTML(R"HTML(
    <style>
      @page { margin:0; }
      @page larger { size:15in; }
      html, body { margin:0; height:100%; }
      div { width:100%; height:100%; }
      * { box-sizing:border-box; }
    </style>
    <div style="page:larger;"></div>
    <div style="break-before:page; border:2pt solid #00ff00;"></div>
  )HTML");

  print_settings().Set(kSettingShouldPrintBackgrounds, true);

  printer()->set_should_generate_page_images(true);

  OnPrintPreview();
  const MockPrinterPage* page = printer()->GetPrinterPage(1);
  ASSERT_TRUE(page);
  const printing::Image& image(page->image());

  ASSERT_EQ(image.size(), gfx::Size(612, 792));

  // Find the border in the bottom right corner of the page.
  EXPECT_EQ(image.pixel_at(611, 788), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(611, 789), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(611, 790), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(611, 791), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(610, 791), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(609, 791), 0x00ff00U);
  EXPECT_EQ(image.pixel_at(608, 791), 0x00ff00U);
}

#endif  // MOCK_PRINTER_SUPPORTS_PAGE_IMAGES

class PrintRenderFrameHelperTaggedPreviewTest
    : public PrintRenderFrameHelperPreviewTest,
      public testing::WithParamInterface<bool> {
 public:
  PrintRenderFrameHelperTaggedPreviewTest() = default;
  PrintRenderFrameHelperTaggedPreviewTest(
      const PrintRenderFrameHelperTaggedPreviewTest&) = delete;
  PrintRenderFrameHelperTaggedPreviewTest& operator=(
      const PrintRenderFrameHelperTaggedPreviewTest&) = delete;
  ~PrintRenderFrameHelperTaggedPreviewTest() override = default;

  // content::RenderViewTest:
  content::ContentRendererClient* CreateContentRendererClient() override {
    return new PrintTestContentRendererClient(GenerateTaggedPDFs());
  }

  bool GenerateTaggedPDFs() const { return GetParam(); }
  bool ExpectsSetAccessibilityTreeCalls() const { return GenerateTaggedPDFs(); }
};

TEST_P(PrintRenderFrameHelperTaggedPreviewTest,
       PrintPreviewRerenderGeneratesTaggedPDF) {
  LoadHTML(kHelloWorldHTML);

  OnPrintPreview();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(548, 692, 72, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  int expected_accessibility_tree_set_count =
      ExpectsSetAccessibilityTreeCalls() ? 1 : 0;
  EXPECT_EQ(expected_accessibility_tree_set_count,
            print_manager()->accessibility_tree_set_count());

  print_settings().Set(kSettingScaleFactor, 200);
  OnPrintPreviewRerender();

  EXPECT_EQ(0u, preview_ui()->print_preview_pages_remaining());
  VerifyDidPreviewPage(true, 0);
  VerifyPreviewPageCount(1);
  VerifyDefaultPageLayout(548, 692, 72, 28, 36, 28, false, false);
  VerifyPrintPreviewCancelled(false);
  VerifyPrintPreviewFailed(false);
  VerifyPrintPreviewGenerated(true);
  VerifyPagesPrinted(false);

  expected_accessibility_tree_set_count =
      ExpectsSetAccessibilityTreeCalls() ? 2 : 0;
  EXPECT_EQ(expected_accessibility_tree_set_count,
            print_manager()->accessibility_tree_set_count());
}

INSTANTIATE_TEST_SUITE_P(All,
                         PrintRenderFrameHelperTaggedPreviewTest,
                         testing::Bool());

#endif  // BUILDFLAG(ENABLE_PRINT_PREVIEW)

#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace printing
