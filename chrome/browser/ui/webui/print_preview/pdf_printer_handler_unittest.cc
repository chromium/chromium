// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/pdf_printer_handler.h"

#include <optional>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/values_test_util.h"
#include "build/build_config.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/scoped_browser_locale.h"
#include "components/url_formatter/url_formatter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/printing/printer_capabilities_mac.h"
#include "printing/backend/print_backend.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#endif

#define FPL(x) FILE_PATH_LITERAL(x)

namespace printing {

namespace {

const char kPdfDeviceName[] = "Save as PDF";

const char kPdfPrinterCapability[] =
    R"({
        "capabilities":{
          "printer":{
            "color":{
              "option":[
                {
                  "is_default":true,
                  "type":"STANDARD_COLOR",
                  "vendor_id":"2"
                }
              ]
            },
            "dpi": {
               "option": [ {
                  "horizontal_dpi": 300,
                  "is_default": true,
                  "vertical_dpi": 300
               } ]
            },
            "media_size":{
              "option":[
                {
                  "height_microns":1189000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 841000,
                  "imageable_area_top_microns": 1189000,
                  "name":"ISO_A0",
                  "width_microns":841000,
                },
                {
                  "height_microns":841000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 594000,
                  "imageable_area_top_microns": 841000,
                  "name":"ISO_A1",
                  "width_microns":594000,
                },
                {
                  "height_microns":594000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 420000,
                  "imageable_area_top_microns": 594000,
                  "name":"ISO_A2",
                  "width_microns":420000,
                },
                {
                  "height_microns":420000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 297000,
                  "imageable_area_top_microns": 420000,
                  "name":"ISO_A3",
                  "width_microns":297000,
                },
                {
                  "height_microns":297000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 210000,
                  "imageable_area_top_microns": 297000,
                  "name":"ISO_A4",
                  "width_microns":210000,
                },
                {
                  "height_microns":210000,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 148000,
                  "imageable_area_top_microns": 210000,
                  "name":"ISO_A5",
                  "width_microns":148000,
                },
                {
                  "height_microns":355600,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 215900,
                  "imageable_area_top_microns": 355600,
                  "name":"NA_LEGAL",
                  "width_microns":215900,
                },
                {
                  "height_microns":279400,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 215900,
                  "imageable_area_top_microns": 279400,
                  "is_default":true,
                  "name":"NA_LETTER",
                  "width_microns":215900,
                },
                {
                  "height_microns":431800,
                  "imageable_area_bottom_microns": 0,
                  "imageable_area_left_microns": 0,
                  "imageable_area_right_microns": 279400,
                  "imageable_area_top_microns": 431800,
                  "name":"NA_LEDGER",
                  "width_microns":279400,
                }
              ]
            },
            "page_orientation":{
              "option":[
                {
                  "type":"PORTRAIT"
                },
                {
                  "type":"LANDSCAPE"
                },
                {
                  "is_default":true,
                  "type":"AUTO"
                }
              ]
            }
          },
          "version":"1.0"
        },
        "deviceName":"Save as PDF"
      })";

// Used as a callback to StartGetCapability() in tests.
// Records values returned by StartGetCapability().
void RecordCapability(base::OnceClosure done_closure,
                      base::Value::Dict* capability_out,
                      base::Value::Dict capability) {
  *capability_out = std::move(capability);
  std::move(done_closure).Run();
}

#if BUILDFLAG(IS_MAC)
base::Value::Dict GetValueFromCustomPaper(
    const PrinterSemanticCapsAndDefaults::Paper& paper) {
  base::Value::Dict paper_value;
  paper_value.Set("custom_display_name", paper.display_name());
  paper_value.Set("height_microns", paper.size_um().height());
  paper_value.Set("width_microns", paper.size_um().width());
  int imageable_area_left_microns = paper.printable_area_um().x();
  int imageable_area_bottom_microns = paper.printable_area_um().y();
  int imageable_area_right_microns =
      paper.printable_area_um().x() + paper.printable_area_um().width();
  int imageable_area_top_microns =
      paper.printable_area_um().y() + paper.printable_area_um().height();
  paper_value.Set("imageable_area_left_microns", imageable_area_left_microns);
  paper_value.Set("imageable_area_bottom_microns",
                  imageable_area_bottom_microns);
  paper_value.Set("imageable_area_right_microns", imageable_area_right_microns);
  paper_value.Set("imageable_area_top_microns", imageable_area_top_microns);
  return paper_value;
}
#endif

}  // namespace

using PdfPrinterHandlerTest = testing::Test;

class PdfPrinterHandlerGetCapabilityTest : public BrowserWithTestWindowTest {
 public:
  PdfPrinterHandlerGetCapabilityTest() = default;
  ~PdfPrinterHandlerGetCapabilityTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // Set the locale to ensure NA_LETTER is the default paper size.
    scoped_browser_locale_ = std::make_unique<ScopedBrowserLocale>("en-US");

    // Create the PDF printer handler
    pdf_printer_handler_ = std::make_unique<PdfPrinterHandler>(
        profile(), browser()->tab_strip_model()->GetActiveWebContents(),
        /*sticky_settings=*/nullptr);
  }

 protected:
  base::Value::Dict StartGetCapabilityAndWaitForResults() {
    base::RunLoop run_loop;
    base::Value::Dict capability;
    pdf_printer_handler_->StartGetCapability(
        kPdfDeviceName,
        base::BindOnce(&RecordCapability, run_loop.QuitClosure(), &capability));
    run_loop.Run();

    return capability;
  }

 private:
  std::unique_ptr<ScopedBrowserLocale> scoped_browser_locale_;
  std::unique_ptr<PdfPrinterHandler> pdf_printer_handler_;
};

TEST_F(PdfPrinterHandlerTest, GetFileNameForPrintJobTitle) {
  static const struct {
    const char* input;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"Foo", FPL("Foo.pdf")},
      {"bar", FPL("bar.pdf")},
      {"qux.html", FPL("qux.html.pdf")},
      {"qux.pdf", FPL("qux.pdf")},
      {"Print Me", FPL("Print Me.pdf")},
      {"Print Me.html", FPL("Print Me.html.pdf")},
      {"1l!egal_F@L#(N)ame.html", FPL("1l!egal_F@L#(N)ame.html.pdf")},
      {"example.com", FPL("example.com.pdf")},
      {"data:text/html,foo", FPL("data_text_html,foo.pdf")},
      {"Baz.com Mail - this is e-mail - what. does it mean",
       FPL("Baz.com Mail - this is e-mail - what. does it mean.pdf")},
      {"Baz.com Mail - this is email - what. does. it. mean?",
       FPL("Baz.com Mail - this is email - what. does. it. mean_.pdf")},
      {"Baz.com Mail - This is email. What does it mean.",
       FPL("Baz.com Mail - This is email. What does it mean_.pdf")},
      {"Baz.com Mail - this is email what does it mean",
       FPL("Baz.com Mail - this is email what does it mean.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.input);
    base::FilePath path = PdfPrinterHandler::GetFileNameForPrintJobTitle(
        base::ASCIIToUTF16(data.input));
    EXPECT_EQ(data.expected_output, path.value());
  }
}

TEST_F(PdfPrinterHandlerTest, GetFileNameForPrintJobURL) {
  static const struct {
    const char* input;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"http://example.com", FPL("example.com.pdf")},
      {"http://example.com/?foo", FPL("example.com.pdf")},
      {"https://example.com/foo.html", FPL("foo.pdf")},
      {"https://example.com/bar/qux.txt", FPL("qux.pdf")},
      {"https://example.com/bar/qux.pdf", FPL("qux.pdf")},
      {"data:text/html,foo", FPL("dataurl.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(data.input);
    base::FilePath path =
        PdfPrinterHandler::GetFileNameForURL(GURL(data.input));
    EXPECT_EQ(data.expected_output, path.value());
  }
}

TEST_F(PdfPrinterHandlerTest, GetFileName) {
  static const struct {
    const char* url;
    const char* job_title;
    bool is_savable;
    const base::FilePath::CharType* expected_output;
  } kTestData[] = {
      {"http://example.com", "Example Website", true,
       FPL("Example Website.pdf")},
      {"http://example.com/foo.html", "Website", true, FPL("Website.pdf")},
      {"http://example.com/foo.html", "Print Me.html", true,
       FPL("Print Me.html.pdf")},
      {"http://mail.google.com/mail/u/0/#inbox/hash",
       "Baz.com Mail - This is email. What does it mean.", true,
       FPL("Baz.com Mail - This is email. What does it mean_.pdf")},
      {"data:text/html,foo", "data:text/html,foo", true, FPL("dataurl.pdf")},
      {"data:text/html,<title>someone@example.com", "someone@example.com", true,
       FPL("someone@example.com.pdf")},
      {"file:///tmp/test.png", "test.png (420x150)", false, FPL("test.pdf")},
      {"http://empty.com", "", true, FPL("empty.com.pdf")},
      {"http://empty.com/image", "", false, FPL("image.pdf")},
      {"http://empty.com/nomimetype", "", false, FPL("nomimetype.pdf")},
      {"http://empty.com/weird.extension", "", false, FPL("weird.pdf")},
      {"chrome-extension://foo/views/app.html", "demo.docx", true,
       FPL("demo.docx.pdf")},
  };

  for (const auto& data : kTestData) {
    SCOPED_TRACE(std::string(data.url) + " | " + data.job_title);
    GURL url(data.url);
    std::u16string job_title = base::ASCIIToUTF16(data.job_title);
    base::FilePath path =
        PdfPrinterHandler::GetFileName(url, job_title, data.is_savable);
    EXPECT_EQ(data.expected_output, path.value());
  }
}

TEST_F(PdfPrinterHandlerGetCapabilityTest, GetCapability) {
  base::Value expected_capability =
      base::test::ParseJson(kPdfPrinterCapability);
  ASSERT_TRUE(expected_capability.is_dict());
  base::Value::Dict capability = StartGetCapabilityAndWaitForResults();
  EXPECT_EQ(expected_capability.GetDict(), capability);
}

#if BUILDFLAG(IS_MAC)
TEST_F(PdfPrinterHandlerGetCapabilityTest,
       GetMacCustomPaperSizesInCapabilities) {
  constexpr char kPaperOptionPath[] = "capabilities.printer.media_size.option";
  // The first Paper has an arbitrary valid printable area. The rest have
  // printable area that match the size.
  static const PrinterSemanticCapsAndDefaults::Papers kTestPapers = {
      {"printer1", "", gfx::Size(101600, 127000),
       gfx::Rect(120, 120, 101480, 126880)},
      {"printer2", "", gfx::Size(76200, 152400),
       gfx::Rect(0, 0, 76200, 152400)},
      {"printer3", "", gfx::Size(330200, 863600),
       gfx::Rect(0, 0, 330200, 863600)},
      {"printer4", "", gfx::Size(101600, 50800),
       gfx::Rect(0, 0, 101600, 50800)},
  };

  base::Value expected_capability =
      base::test::ParseJson(kPdfPrinterCapability);
  ASSERT_TRUE(expected_capability.is_dict());

  base::Value::List* expected_paper_options =
      expected_capability.GetDict().FindListByDottedPath(kPaperOptionPath);
  ASSERT_TRUE(expected_paper_options);

  for (const PrinterSemanticCapsAndDefaults::Paper& paper : kTestPapers)
    expected_paper_options->Append(GetValueFromCustomPaper(paper));

  SetMacCustomPaperSizesForTesting(kTestPapers);

  base::Value::Dict capability = StartGetCapabilityAndWaitForResults();

  const base::Value::List* paper_options =
      capability.FindListByDottedPath(kPaperOptionPath);
  ASSERT_TRUE(paper_options);
  EXPECT_EQ(*expected_paper_options, *paper_options);
}
#endif

}  // namespace printing
