// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/printing/browser_printing_context_factory_for_test.h"
#include "chrome/browser/printing/print_error_dialog.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_view_manager_common.h"
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/printing/test_print_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/read_anything/read_anything_enums.h"
#include "chrome/browser/ui/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_view.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/test_print_backend.h"
#include "printing/printing_context.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"

namespace {

// Returns the name of the PDF accessibility node for a line of `text`.
// Lines end in CRLF while kPdfAccessibilityHeuristicEnhancements is off.
std::string LineNodeName(std::string_view text) {
  return base::StrCat({text, "\r\n"});
}

std::optional<uint32_t> GetTextColor(content::WebContents* preview_dialog,
                                     std::string_view name) {
  content::FindAccessibilityNodeCriteria criteria;
  criteria.role = ax::mojom::Role::kStaticText;
  criteria.name = std::string(name);
  ui::AXPlatformNodeDelegate* delegate =
      content::FindAccessibilityNode(preview_dialog, criteria);
  if (!delegate) {
    return std::nullopt;
  }
  auto* node = ui::BrowserAccessibility::FromAXPlatformNodeDelegate(delegate);
  if (!node || node->InternalChildCount() == 0) {
    return std::nullopt;
  }
  // The PDF accessibility tree puts the text color on the inline text box.
  ui::BrowserAccessibility* inline_text_box = node->InternalGetChild(0);
  if (!inline_text_box ||
      !inline_text_box->HasIntAttribute(ax::mojom::IntAttribute::kColor)) {
    return std::nullopt;
  }
  return static_cast<uint32_t>(
      inline_text_box->GetIntAttribute(ax::mojom::IntAttribute::kColor));
}

}  // namespace

// Base for the IRM printing fixtures below.
class ReadAnythingPrintingTestBase : public InProcessBrowserTest {
 public:
  static constexpr char kArticlePath[] =
      "/reading_mode/multi_page_article.html";
  static constexpr char kTopSentinel[] = "TOP_SENTINEL_ALPHA_12345";
  static constexpr char kBottomSentinel[] = "BOTTOM_SENTINEL_OMEGA_67890";

  // InProcessBrowserTest:
  void SetUp() override {
    test_print_backend_ = base::MakeRefCounted<printing::TestPrintBackend>();
    printing::PrintBackend::SetPrintBackendForTesting(
        test_print_backend_.get());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetProfile()->GetPrefs()->ClearPref(
        prefs::kAccessibilityReadAnythingLastOpenedPresentationState);
    printing::PrintingContext::SetPrintingContextFactoryForTest(
        &test_printing_context_factory_);
    AddPrinter("printer1");
    test_printing_context_factory_.SetPrinterNameForSubsequentContexts(
        "printer1");
    SetShowPrintErrorDialogForTest(base::DoNothing());
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    SetShowPrintErrorDialogForTest(base::NullCallback());
    printing::PrintingContext::SetPrintingContextFactoryForTest(nullptr);
    if (content::WebContents* irm_contents = GetImmersiveWebContents()) {
      if (auto* manager =
              web_modal::WebContentsModalDialogManager::FromWebContents(
                  irm_contents)) {
        manager->CloseAllDialogs();
      }
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    printing::PrintBackend::SetPrintBackendForTesting(nullptr);
  }

  void NavigateToArticle() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(kArticlePath)));
  }

  tabs::TabInterface* GetActiveTab() {
    return browser()->GetTabStripModel()->GetActiveTab();
  }

  // Shows IRM on the active tab and waits until its app has loaded.
  content::WebContents* OpenIRM() {
    tabs::TabInterface* tab = GetActiveTab();
    auto* controller = ReadAnythingController::From(tab);
    CHECK(controller);

    controller->ShowImmersiveUI(
        read_anything::mojom::ReadAnythingOpenTrigger::kOmniboxChip);

    // Print commands must not target the IRM contents before the overlay is
    // visible.
    views::View* overlay_view = GetActiveImmersiveOverlay();
    if (!overlay_view || !overlay_view->GetVisible()) {
      EXPECT_EQ(printing::GetWebContentsToPrint(tab), tab->GetContents());
    }

    EXPECT_TRUE(base::test::RunUntil(
        [&]() { return overlay_view && overlay_view->GetVisible(); }));

    content::WebContents* irm_contents = GetImmersiveWebContents();
    CHECK(irm_contents);
    EXPECT_TRUE(content::WaitForLoadStop(irm_contents));
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(irm_contents,
                             "!!document.querySelector('read-anything-app')"
                             "?.shadowRoot?.querySelector('#container')")
          .ExtractBool();
    }));
    return irm_contents;
  }

  // Waits until IRM shows the whole article.
  void WaitForDistillation(content::WebContents* irm_contents) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(irm_contents,
                             content::JsReplace(
                                 "document.querySelector('read-anything-app')"
                                 "?.shadowRoot?.querySelector('#container')"
                                 "?.innerText.includes($1) ?? false",
                                 kBottomSentinel))
          .ExtractBool();
    }));
  }

  // Runs IDC_PRINT and checks that the preview is for the tab's page, not for
  // `irm_contents`.
  void PrintAndExpectPreviewForTab(tabs::TabInterface* tab,
                                   content::WebContents* irm_contents) {
    printing::TestPrintPreviewObserver observer(/*wait_for_loaded=*/false);
    chrome::ExecuteCommand(browser(), IDC_PRINT);
    observer.WaitUntilPreviewIsReady();
    auto* dialog_controller =
        printing::PrintPreviewDialogController::GetInstance();
    ASSERT_TRUE(dialog_controller);
    EXPECT_TRUE(
        dialog_controller->GetPrintPreviewForContents(tab->GetContents()));
    EXPECT_FALSE(dialog_controller->GetPrintPreviewForContents(irm_contents));
  }

 private:
  views::View* GetActiveImmersiveOverlay() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    return browser_view
        ->GetContentsContainerViewFor(GetActiveTab()->GetContents())
        ->GetViewByID(VIEW_ID_READ_ANYTHING_OVERLAY);
  }

  // Returns the IRM WebContents, or null if IRM is not showing.
  content::WebContents* GetImmersiveWebContents() {
    views::View* overlay_view = GetActiveImmersiveOverlay();
    if (!overlay_view || !overlay_view->GetVisible() ||
        overlay_view->children().empty()) {
      return nullptr;
    }
    views::WebView* web_view =
        static_cast<views::WebView*>(overlay_view->children()[0]);
    return web_view->GetWebContents();
  }

  void AddPrinter(const std::string& printer_name) {
    printing::PrinterBasicInfo printer_info(
        printer_name, /*display_name=*/"test printer",
        /*printer_description=*/"A printer for testing.",
        printing::test::GetPrintInfoOptions());

    auto default_caps =
        std::make_unique<printing::PrinterSemanticCapsAndDefaults>();
    default_caps->copies_max = 1;
    default_caps->dpis = printing::test::GetPrinterCapabilitiesDefaultDpis();
    default_caps->default_dpi = printing::test::kPrinterCapabilitiesDpi;
    default_caps->papers.push_back(printing::test::GetPaperLetter());
    test_print_backend_->AddValidPrinter(
        printer_name, std::move(default_caps),
        std::make_unique<printing::PrinterBasicInfo>(printer_info));
    test_print_backend_->SetDefaultPrinterName(printer_name);
  }

  scoped_refptr<printing::TestPrintBackend> test_print_backend_;
  printing::BrowserPrintingContextFactoryForTest test_printing_context_factory_;
};

class ReadAnythingPrintingBrowserTest : public ReadAnythingPrintingTestBase {
 public:
  ReadAnythingPrintingBrowserTest() {
    // TODO(crbug.com/467180032): When kPdfAccessibilityHeuristicEnhancements
    // is enabled by default, stop disabling it and update LineNodeName(). The
    // accessibility node names in these tests assume it is disabled.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kImmersiveReadAnythingPrint},
        /*disabled_features=*/{
            features::kPdfAccessibilityHeuristicEnhancements});
  }

  // Must be called before `contents` makes its first print call: the renderer
  // binds to the print manager lazily and doesn't rebind.
  printing::TestPrintViewManager* UseTestPrintViewManager(
      content::WebContents* contents) {
    return printing::TestPrintViewManager::CreateForWebContents(contents);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Prints IRM and checks that the output has all of the distilled content and
// the Reading Mode theme.
IN_PROC_BROWSER_TEST_F(ReadAnythingPrintingBrowserTest,
                       PrintsFullStyledIrmContent) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  NavigateToArticle();
  browser()->GetProfile()->GetPrefs()->SetInteger(
      prefs::kAccessibilityReadAnythingColorInfo,
      static_cast<int>(read_anything::mojom::Colors::kDark));

  content::WebContents* irm_contents = OpenIRM();
  ASSERT_TRUE(irm_contents);
  WaitForDistillation(irm_contents);

  printing::TestPrintPreviewObserver observer(/*wait_for_loaded=*/false);
  chrome::ExecuteCommand(browser(), IDC_PRINT);
  observer.WaitUntilPreviewIsReady();

  auto* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);
  EXPECT_TRUE(dialog_controller->GetPrintPreviewForContents(irm_contents));
  EXPECT_FALSE(dialog_controller->GetPrintPreviewForContents(
      GetActiveTab()->GetContents()));

  EXPECT_GE(observer.rendered_page_count(), 2u);
  content::WebContents* preview_dialog =
      dialog_controller->GetPrintPreviewForContents(irm_contents);
  ASSERT_TRUE(preview_dialog);
  content::WaitForAccessibilityTreeToContainNodeWithName(
      preview_dialog, LineNodeName(kBottomSentinel));
  content::WaitForAccessibilityTreeToContainNodeWithName(
      preview_dialog, LineNodeName(kTopSentinel));

  // The Dark theme draws light text. Printing darkens light text unless the
  // print CSS sets `print-color-adjust: exact`.
  std::optional<uint32_t> color =
      GetTextColor(preview_dialog, LineNodeName(kBottomSentinel));
  ASSERT_TRUE(color.has_value());
  uint8_t a = (*color >> 24) & 0xFF;
  uint8_t r = (*color >> 16) & 0xFF;
  uint8_t g = (*color >> 8) & 0xFF;
  uint8_t b = *color & 0xFF;
  EXPECT_GE(a, 0xC0u);
  EXPECT_GE(r, 0xC0u);
  EXPECT_GE(g, 0xC0u);
  EXPECT_GE(b, 0xC0u);
}

// Closing IRM with a preview open closes the preview and restores the print
// target, and IRM can print again after it is reopened.
IN_PROC_BROWSER_TEST_F(ReadAnythingPrintingBrowserTest,
                       CloseWithPreviewOpenThenReopenPrintsAgain) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  NavigateToArticle();
  tabs::TabInterface* tab = GetActiveTab();
  auto* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  content::WebContents* irm_contents = OpenIRM();
  ASSERT_TRUE(irm_contents);
  WaitForDistillation(irm_contents);
  {
    printing::TestPrintPreviewObserver observer(/*wait_for_loaded=*/false);
    chrome::ExecuteCommand(browser(), IDC_PRINT);
    observer.WaitUntilPreviewIsReady();
  }

  content::WebContents* old_dialog =
      dialog_controller->GetPrintPreviewForContents(irm_contents);
  ASSERT_TRUE(old_dialog);
  content::WebContentsDestroyedWatcher watcher(old_dialog);

  ReadAnythingController::From(tab)->CloseImmersiveUI(
      ReadAnythingCloseReason::kClosedByUser);

  EXPECT_EQ(printing::GetWebContentsToPrint(tab), tab->GetContents());
  // The preview closes asynchronously. Wait until it is destroyed, which also
  // keeps callbacks from the old dialog from reaching the next observer.
  watcher.Wait();
  EXPECT_EQ(dialog_controller->GetPrintPreviewForContents(irm_contents),
            nullptr);

  ASSERT_EQ(OpenIRM(), irm_contents);
  WaitForDistillation(irm_contents);

  printing::TestPrintPreviewObserver observer(/*wait_for_loaded=*/false);
  chrome::ExecuteCommand(browser(), IDC_PRINT);
  observer.WaitUntilPreviewIsReady();

  EXPECT_TRUE(dialog_controller->GetPrintPreviewForContents(irm_contents));
  EXPECT_FALSE(
      dialog_controller->GetPrintPreviewForContents(tab->GetContents()));

  EXPECT_GE(observer.rendered_page_count(), 2u);
  content::WebContents* preview_dialog =
      dialog_controller->GetPrintPreviewForContents(irm_contents);
  ASSERT_TRUE(preview_dialog);
  content::WaitForAccessibilityTreeToContainNodeWithName(
      preview_dialog, LineNodeName(kBottomSentinel));
}

// After IRM closes, Print prints the page again.
IN_PROC_BROWSER_TEST_F(ReadAnythingPrintingBrowserTest,
                       PrintsPageAfterIrmCloses) {
  NavigateToArticle();
  tabs::TabInterface* tab = GetActiveTab();
  content::WebContents* irm_contents = OpenIRM();
  ASSERT_TRUE(irm_contents);

  ReadAnythingController::From(tab)->CloseImmersiveUI(
      ReadAnythingCloseReason::kClosedByUser);

  PrintAndExpectPreviewForTab(tab, irm_contents);
}

// If a user requests print and closes IRM before preview arrives, the request
// is dropped and does not show or crash, and IRM can print again after it is
// reopened.
IN_PROC_BROWSER_TEST_F(ReadAnythingPrintingBrowserTest,
                       PrintRequestWhileClosingIrmIsDroppedThenReopenPrints) {
  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  auto* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);
  NavigateToArticle();
  tabs::TabInterface* tab = GetActiveTab();

  content::WebContents* irm_contents = OpenIRM();
  ASSERT_TRUE(irm_contents);
  WaitForDistillation(irm_contents);
  auto* test_pvm = UseTestPrintViewManager(irm_contents);

  chrome::ExecuteCommand(browser(), IDC_PRINT);
  chrome::ExecuteCommand(browser(), IDC_SHOW_READING_MODE_KEYBOARD);

  EXPECT_EQ(printing::GetWebContentsToPrint(tab), tab->GetContents());
  test_pvm->WaitUntilPreviewIsShownOrCancelled();

  EXPECT_EQ(dialog_controller->GetPrintPreviewForContents(irm_contents),
            nullptr);
  EXPECT_EQ(dialog_controller->GetPrintPreviewForContents(tab->GetContents()),
            nullptr);

  ASSERT_EQ(OpenIRM(), irm_contents);
  WaitForDistillation(irm_contents);

  printing::TestPrintPreviewObserver observer(/*wait_for_loaded=*/false);
  chrome::ExecuteCommand(browser(), IDC_PRINT);
  observer.WaitUntilPreviewIsReady();

  EXPECT_TRUE(dialog_controller->GetPrintPreviewForContents(irm_contents));
  EXPECT_FALSE(
      dialog_controller->GetPrintPreviewForContents(tab->GetContents()));

  EXPECT_GE(observer.rendered_page_count(), 2u);
  content::WebContents* preview_dialog =
      dialog_controller->GetPrintPreviewForContents(irm_contents);
  ASSERT_TRUE(preview_dialog);
  content::WaitForAccessibilityTreeToContainNodeWithName(
      preview_dialog, LineNodeName(kBottomSentinel));
}

class ReadAnythingPrintingDisabledBrowserTest
    : public ReadAnythingPrintingTestBase {
 public:
  ReadAnythingPrintingDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(features::kImmersiveReadAnythingPrint);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// With kImmersiveReadAnythingPrint disabled, IRM gets no PrintViewManager and
// print commands target the tab's page.
IN_PROC_BROWSER_TEST_F(ReadAnythingPrintingDisabledBrowserTest,
                       FeatureFlagDisabledRegression) {
  NavigateToArticle();

  content::WebContents* irm_contents = OpenIRM();
  ASSERT_TRUE(irm_contents);

  // With flag disabled, PrintViewManager is not attached to the IRM contents.
  EXPECT_EQ(printing::PrintViewManager::FromWebContents(irm_contents), nullptr);

  tabs::TabInterface* tab = GetActiveTab();
  EXPECT_EQ(printing::GetWebContentsToPrint(tab), tab->GetContents());

  // After IRM closes, Print prints the page.
  ReadAnythingController::From(tab)->CloseImmersiveUI(
      ReadAnythingCloseReason::kClosedByUser);
  PrintAndExpectPreviewForTab(tab, irm_contents);
}
