// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/browser_test.h"
#include "extensions/common/extension_features.h"
#include "extensions/test/result_catcher.h"
#include "extensions/test/test_extension_dir.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/events/test/test_event.h"

namespace extensions {

namespace {

constexpr char16_t kMimeHandlerExtensionName[] = u"Generic MIME Handler Test";
constexpr char kTestExtensionDir[] = "generic_mime_handler";
constexpr char kTestPdfPath[] = "/test.pdf";
constexpr char kEmptyPath[] = "/empty.html";
constexpr char kEmbedHostPath[] = "/embed_host.html";
constexpr char16_t kPdfOwnerExtensionName[] = u"With PDF";

base::FilePath PdfOwnerExtensionDir() {
  return base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
      .AppendASCII("pdf/extension_with_pdf");
}

LocationIconView* GetLocationIconView(const Browser* browser) {
  return BrowserView::GetBrowserViewForBrowser(browser)
      ->GetLocationBarView()
      ->location_icon_view();
}

void ExpectExtensionChipShowing(Browser* browser, std::u16string_view name) {
  LocationIconView* icon = GetLocationIconView(browser);
  EXPECT_TRUE(icon->GetShowText());
  EXPECT_EQ(name, icon->GetText());
}

void ExpectExtensionChipNotShowing(Browser* browser, std::u16string_view name) {
  LocationIconView* icon = GetLocationIconView(browser);
  EXPECT_NE(name, icon->GetText());
}

}  // namespace

class GenericMimeHandlerChipBrowserTest : public ExtensionApiTest {
 protected:
  GenericMimeHandlerChipBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiMimeHandler},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    const base::FilePath test_data_dir =
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA);
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("pdf"));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII(kTestExtensionDir));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Navigating to a top-level MIME-handled resource surfaces the extension
// name in the omnibox chip.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorVisibleOnTopLevelMimeHandler) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExpectExtensionChipShowing(browser(), kMimeHandlerExtensionName);
}

// Navigating away from a MIME-handled page clears the chip label.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorClearedOnNavigationAway) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kEmptyPath)));

  ExpectExtensionChipNotShowing(browser(), kMimeHandlerExtensionName);
}

// An embedded MIME handler (inside <embed> or <iframe>) does not
// relabel the chip, since the host page is the tab's identity.
// The fixture's manifest sets `can_embed: true`, so the embedded handler
// actually runs; the chip retains the host page's identity rather than the
// extension name.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorHiddenForEmbeddedHandler) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kEmbedHostPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExpectExtensionChipNotShowing(browser(), kMimeHandlerExtensionName);
}

// When the MIME-handled resource is served from `chrome-extension://`,
// the chip names the URL-owner extension, not the handler extension.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorNamesOwnerExtensionForExtensionServedPdf) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));
  const Extension* owner_extension = LoadExtension(PdfOwnerExtensionDir());
  ASSERT_TRUE(owner_extension);

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            owner_extension->GetResourceURL("test.pdf")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExpectExtensionChipShowing(browser(), kPdfOwnerExtensionName);
}

// When the MIME-handler extension also owns the URL (self-served PDF
// bundled inside the handler extension itself), the chip names that
// extension.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorNamesExtensionForSelfServedPdf) {
  const Extension* handler_extension =
      LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir));
  ASSERT_TRUE(handler_extension);

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            handler_extension->GetResourceURL("test.pdf")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExpectExtensionChipShowing(browser(), kMimeHandlerExtensionName);
}

// When a chrome-extension:// page embeds a PDF via <iframe> that
// the MIME handler intercepts, the chip names the URL-owner
// extension of the top-level frame, not the handler.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       IndicatorNamesOwnerForEmbeddedExtensionPdf) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  TestExtensionDir owner_dir;
  owner_dir.WriteManifest(R"({
    "name": "PDF Owner Page",
    "version": "1.0",
    "manifest_version": 3
  })");
  owner_dir.WriteFile(FILE_PATH_LITERAL("host.html"),
                      R"(<!DOCTYPE html><iframe src="bundled.pdf"></iframe>)");
  owner_dir.CopyFileTo(
      test_data_dir_.AppendASCII(kTestExtensionDir).AppendASCII("test.pdf"),
      FILE_PATH_LITERAL("bundled.pdf"));

  const Extension* owner_extension = LoadExtension(owner_dir.UnpackedPath());
  ASSERT_TRUE(owner_extension);

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            owner_extension->GetResourceURL("host.html")));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  ExpectExtensionChipShowing(browser(), u"PDF Owner Page");
}

// Pressing the chip on a top-level MIME handler page opens the
// minimal InternalPageInfoBubbleView ("Extension" label), matching
// the chrome-extension:// click behavior, instead of the regular
// PageInfoBubbleView with site-info / security controls.
IN_PROC_BROWSER_TEST_F(GenericMimeHandlerChipBrowserTest,
                       ChipClickOpensInternalPageInfo) {
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII(kTestExtensionDir)));

  ResultCatcher catcher;
  ASSERT_TRUE(NavigateToURL(GetActiveWebContents(),
                            embedded_test_server()->GetURL(kTestPdfPath)));
  ASSERT_TRUE(catcher.GetNextResult()) << catcher.message();

  LocationIconView* icon = GetLocationIconView(browser());
  ui::test::TestEvent event;
  ASSERT_TRUE(icon->ShowBubble(event));

  EXPECT_EQ(PageInfoBubbleViewBase::BUBBLE_INTERNAL_PAGE,
            PageInfoBubbleViewBase::GetShownBubbleType());
}

}  // namespace extensions
