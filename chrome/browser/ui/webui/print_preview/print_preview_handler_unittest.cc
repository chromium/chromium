// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/webui/print_preview/fake_print_render_frame.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

const char kDummyInitiatorName[] = "TestInitiator";
const char kEmptyPrinterName[] = "EmptyPrinter";
const char kTestData[] = "abc";

// Array of all PrinterTypes.
constexpr PrinterType kAllTypes[] = {PrinterType::kPrivet,
                                     PrinterType::kExtension, PrinterType::kPdf,
                                     PrinterType::kLocal, PrinterType::kCloud};

// Array of all PrinterTypes that have working PrinterHandlers.
constexpr PrinterType kAllSupportedTypes[] = {
    PrinterType::kPrivet, PrinterType::kExtension, PrinterType::kPdf,
    PrinterType::kLocal};

// All three printer types that implement PrinterHandler::StartGetPrinters().
constexpr PrinterType kFetchableTypes[] = {
    PrinterType::kPrivet, PrinterType::kExtension, PrinterType::kLocal};

struct PrinterInfo {
  std::string id;
  bool is_default;
  base::Value basic_info = base::Value(base::Value::Type::DICTIONARY);
  base::Value capabilities = base::Value(base::Value::Type::DICTIONARY);
};

PrinterInfo GetSimplePrinterInfo(const std::string& name, bool is_default) {
  PrinterInfo simple_printer;
  simple_printer.id = name;
  simple_printer.is_default = is_default;
  simple_printer.basic_info.SetKey("printer_name",
                                   base::Value(simple_printer.id));
  simple_printer.basic_info.SetKey("printer_description",
                                   base::Value("Printer for test"));
  simple_printer.basic_info.SetKey("printer_status", base::Value(1));
  base::Value cdd(base::Value::Type::DICTIONARY);
  base::Value capabilities(base::Value::Type::DICTIONARY);
  simple_printer.capabilities.SetKey("printer",
                                     simple_printer.basic_info.Clone());
  simple_printer.capabilities.SetKey("capabilities", cdd.Clone());
  return simple_printer;
}

PrinterInfo GetEmptyPrinterInfo() {
  PrinterInfo empty_printer;
  empty_printer.id = kEmptyPrinterName;
  empty_printer.is_default = false;
  empty_printer.basic_info.SetKey("printer_name",
                                  base::Value(empty_printer.id));
  empty_printer.basic_info.SetKey("printer_description",
                                  base::Value("Printer with no capabilities"));
  empty_printer.basic_info.SetKey("printer_status", base::Value(0));
  empty_printer.capabilities.SetKey("printer",
                                    empty_printer.basic_info.Clone());
  return empty_printer;
}

base::Value GetPrintPreviewTicket() {
  base::Value print_ticket = GetPrintTicket(PrinterType::kLocal);

  // Make some modifications to match a preview print ticket.
  print_ticket.SetKey(kSettingPageRange, base::Value());
  print_ticket.SetBoolKey(kIsFirstRequest, true);
  print_ticket.SetIntKey(kPreviewRequestID, 0);
  print_ticket.SetBoolKey(kSettingPreviewModifiable, false);
  print_ticket.SetBoolKey(kSettingPreviewIsPdf, true);
  print_ticket.RemoveKey(kSettingPageWidth);
  print_ticket.RemoveKey(kSettingPageHeight);
  print_ticket.RemoveKey(kSettingShowSystemDialog);

  return print_ticket;
}

std::unique_ptr<base::ListValue> ConstructPreviewArgs(
    base::StringPiece callback_id,
    const base::Value& print_ticket) {
  base::Value args(base::Value::Type::LIST);
  args.Append(callback_id);
  std::string json;
  base::JSONWriter::Write(print_ticket, &json);
  args.Append(json);
  return base::ListValue::From(base::Value::ToUniquePtrValue(std::move(args)));
}

UserActionBuckets GetUserActionForPrinterType(PrinterType type) {
  switch (type) {
    case PrinterType::kPrivet:
      return UserActionBuckets::kPrintWithPrivet;
    case PrinterType::kExtension:
      return UserActionBuckets::kPrintWithExtension;
    case PrinterType::kPdf:
      return UserActionBuckets::kPrintToPdf;
    case PrinterType::kLocal:
      return UserActionBuckets::kPrintToPrinter;
    case PrinterType::kCloud:
      return UserActionBuckets::kPrintWithCloudPrint;
  }
}

// Checks whether |histograms| was updated correctly by a job with a printer
// type |type| with arguments generated by GetPrintTicket().
void CheckHistograms(const base::HistogramTester& histograms,
                     PrinterType type) {
  static constexpr PrintSettingsBuckets kPopulatedPrintSettingsBuckets[] = {
      PrintSettingsBuckets::kPortrait, PrintSettingsBuckets::kColor,
      PrintSettingsBuckets::kCollate,  PrintSettingsBuckets::kDuplex,
      PrintSettingsBuckets::kTotal,    PrintSettingsBuckets::kDefaultMedia,
  };

  for (auto bucket : kPopulatedPrintSettingsBuckets)
    histograms.ExpectBucketCount("PrintPreview.PrintSettings", bucket, 1);

  // All other PrintPreview.PrintSettings buckets should be empty.
  histograms.ExpectTotalCount("PrintPreview.PrintSettings",
                              base::size(kPopulatedPrintSettingsBuckets));

  const UserActionBuckets user_action = GetUserActionForPrinterType(type);
  histograms.ExpectBucketCount("PrintPreview.UserAction", user_action, 1);
  // Only one PrintPreview.UserAction bucket should have been populated.
  histograms.ExpectTotalCount("PrintPreview.UserAction", 1);

  histograms.ExpectTotalCount("PrintPreview.PrintDocumentSize.HTML", 1);
  histograms.ExpectTotalCount("PrintPreview.PrintDocumentSize.PDF", 0);
  histograms.ExpectBucketCount("PrintPreview.PrintDocumentType",
                               PrintDocumentTypeBuckets::kHtmlDocument, 1);
  histograms.ExpectBucketCount("PrintPreview.PrintDocumentType",
                               PrintDocumentTypeBuckets::kPdfDocument, 0);
}

class TestPrinterHandler : public PrinterHandler {
 public:
  explicit TestPrinterHandler(const std::vector<PrinterInfo>& printers) {
    SetPrinters(printers);
  }

  ~TestPrinterHandler() override {}

  void Reset() override {}

  void GetDefaultPrinter(DefaultPrinterCallback cb) override {
    std::move(cb).Run(default_printer_);
  }

  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override {
    if (!printers_.empty())
      added_printers_callback.Run(printers_);
    std::move(done_callback).Run();
  }

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {
    std::move(callback).Run(printer_capabilities_[destination_id]->Clone());
  }

  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override {}

  void StartPrint(const std::u16string& job_title,
                  base::Value settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override {
    std::move(callback).Run(base::Value());
  }

  void SetPrinters(const std::vector<PrinterInfo>& printers) {
    base::Value::ListStorage printer_list;
    for (const auto& printer : printers) {
      if (printer.is_default)
        default_printer_ = printer.id;
      printer_list.push_back(printer.basic_info.Clone());
      printer_capabilities_[printer.id] = base::DictionaryValue::From(
          std::make_unique<base::Value>(printer.capabilities.Clone()));
    }
    printers_ = base::ListValue(printer_list);
  }

 private:
  std::string default_printer_;
  base::ListValue printers_;
  std::map<std::string, std::unique_ptr<base::DictionaryValue>>
      printer_capabilities_;

  DISALLOW_COPY_AND_ASSIGN(TestPrinterHandler);
};

class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  FakePrintPreviewUI(content::WebUI* web_ui,
                     std::unique_ptr<PrintPreviewHandler> handler)
      : PrintPreviewUI(web_ui, std::move(handler)) {}

  ~FakePrintPreviewUI() override {}

  void GetPrintPreviewDataForIndex(
      int index,
      scoped_refptr<base::RefCountedMemory>* data) const override {
    *data = base::MakeRefCounted<base::RefCountedStaticMemory>(
        reinterpret_cast<const unsigned char*>(kTestData),
        sizeof(kTestData) - 1);
  }

  void OnPrintPreviewRequest(int request_id) override {}
  void OnCancelPendingPreviewRequest() override {}
  void OnHidePreviewDialog() override {}
  void OnClosePrintPreviewDialog() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePrintPreviewUI);
};

class TestPrintPreviewPrintRenderFrame : public FakePrintRenderFrame {
 public:
  explicit TestPrintPreviewPrintRenderFrame(
      blink::AssociatedInterfaceProvider* provider)
      : FakePrintRenderFrame(provider) {}

  ~TestPrintPreviewPrintRenderFrame() final = default;

  const base::Value& GetSettings() { return settings_; }

  void SetCompletionClosure(base::OnceClosure closure) {
    closure_ = std::move(closure);
  }

 private:
  // FakePrintRenderFrame:
  void PrintPreview(base::Value settings) final {
    settings_ = std::move(settings);
    std::move(closure_).Run();
  }

  base::OnceClosure closure_;
  base::Value settings_;
};

class TestPrintPreviewHandler : public PrintPreviewHandler {
 public:
  TestPrintPreviewHandler(std::unique_ptr<PrinterHandler> printer_handler,
                          content::WebContents* initiator)
      : bad_messages_(0),
        test_printer_handler_(std::move(printer_handler)),
        initiator_(initiator) {}

  PrinterHandler* GetPrinterHandler(PrinterType printer_type) override {
    called_for_type_.insert(printer_type);
    return test_printer_handler_.get();
  }

  bool IsCloudPrintEnabled() override { return true; }

  void BadMessageReceived() override { bad_messages_++; }

  content::WebContents* GetInitiator() const override { return initiator_; }

  bool CalledOnlyForType(PrinterType printer_type) {
    return (called_for_type_.size() == 1 &&
            *called_for_type_.begin() == printer_type);
  }

  bool NotCalled() { return called_for_type_.empty(); }

  void reset_calls() { called_for_type_.clear(); }

  int bad_messages() { return bad_messages_; }

 private:
  int bad_messages_;
  base::flat_set<PrinterType> called_for_type_;
  std::unique_ptr<PrinterHandler> test_printer_handler_;
  content::WebContents* const initiator_;

  DISALLOW_COPY_AND_ASSIGN(TestPrintPreviewHandler);
};

}  // namespace

class PrintPreviewHandlerTest : public testing::Test {
 public:
  PrintPreviewHandlerTest() = default;
  ~PrintPreviewHandlerTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();
    initiator_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    content::WebContents* initiator = initiator_web_contents_.get();
    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    PrintViewManager::CreateForWebContents(initiator);
    PrintViewManager::FromWebContents(initiator)->PrintPreviewNow(
        initiator->GetMainFrame(), false);
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

    printers_.push_back(GetSimplePrinterInfo(kDummyPrinterName, true));
    auto printer_handler = CreatePrinterHandler(printers_);
    printer_handler_ = printer_handler.get();

    auto preview_handler = std::make_unique<TestPrintPreviewHandler>(
        std::move(printer_handler), initiator);
    preview_handler->set_web_ui(web_ui());
    handler_ = preview_handler.get();

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::move(preview_handler));
    preview_ui->SetInitiatorTitle(base::ASCIIToUTF16(kDummyInitiatorName));
    web_ui()->SetController(std::move(preview_ui));
  }

  void TearDown() override {
    PrintViewManager::FromWebContents(initiator_web_contents_.get())
        ->PrintPreviewDone();
  }

  virtual std::unique_ptr<TestPrinterHandler> CreatePrinterHandler(
      const std::vector<PrinterInfo>& printers) {
    return std::make_unique<TestPrinterHandler>(printers);
  }

  void Initialize() { InitializeWithLocale("en"); }

  void InitializeWithLocale(const std::string& locale) {
    // Sending this message will enable javascript, so it must always be called
    // before any other messages are sent.
    base::Value args(base::Value::Type::LIST);
    args.Append("test-callback-id-0");
    std::unique_ptr<base::ListValue> list_args =
        base::ListValue::From(base::Value::ToUniquePtrValue(std::move(args)));

    auto* browser_process = TestingBrowserProcess::GetGlobal();
    std::string original_locale = browser_process->GetApplicationLocale();
    {
      // Set locale since the delimiters checked in VerifyInitialSettings()
      // depend on it. This has to be done in several ways to make various
      // locale code sync up correctly.
      browser_process->SetApplicationLocale(locale);
      base::test::ScopedRestoreICUDefaultLocale scoped_locale(locale);
      base::ResetFormattersForTesting();
      handler()->HandleGetInitialSettings(list_args.get());
    }
    // Reset again now that |scoped_locale| has been destroyed.
    browser_process->SetApplicationLocale(original_locale);
    base::ResetFormattersForTesting();

    // In response to get initial settings, the initial settings are sent back.
    ASSERT_EQ(1u, web_ui()->call_data().size());
  }

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    std::string event_fired;
    ASSERT_TRUE(data.arg1()->GetAsString(&event_fired));
    EXPECT_EQ(event_id, event_fired);
  }

  void CheckWebUIResponse(const content::TestWebUI::CallData& data,
                          const std::string& callback_id_in,
                          bool expect_success) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    std::string callback_id;
    ASSERT_TRUE(data.arg1()->GetAsString(&callback_id));
    EXPECT_EQ(callback_id_in, callback_id);
    bool success = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&success));
    EXPECT_EQ(expect_success, success);
  }

  void ValidateInitialSettings(const content::TestWebUI::CallData& data,
                               const std::string& default_printer_name,
                               const std::string& initiator_title) {
    ValidateInitialSettingsForLocale(data, default_printer_name,
                                     initiator_title, "en", ",", ".");
  }

  // Validates the initial settings structure in the response matches the
  // print_preview.NativeInitialSettings type in
  // chrome/browser/resources/print_preview/native_layer.js. Checks that:
  //   - |default_printer_name| is the printer name returned
  //   - |initiator_title| is the initiator title returned
  // Also validates that delimiters are correct for |locale| (set in
  // InitializeWithLocale()) with the associated |thousands_delimiter| and
  // |decimal_delimiter|.
  // Assumes "test-callback-id-0" was used as the callback id.
  void ValidateInitialSettingsForLocale(
      const content::TestWebUI::CallData& data,
      const std::string& default_printer_name,
      const std::string& initiator_title,
      const std::string& locale,
      const std::string& thousands_delimiter,
      const std::string& decimal_delimiter) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value* settings = data.arg3();
    ASSERT_TRUE(settings->FindKeyOfType("isInKioskAutoPrintMode",
                                        base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindKeyOfType("isInAppKioskMode",
                                        base::Value::Type::BOOLEAN));

    const std::string* actual_locale = settings->FindStringKey("uiLocale");
    ASSERT_TRUE(actual_locale);
    EXPECT_EQ(locale, *actual_locale);
    const std::string* actual_thousands_delimiter =
        settings->FindStringKey("thousandsDelimiter");
    ASSERT_TRUE(actual_thousands_delimiter);
    EXPECT_EQ(thousands_delimiter, *actual_thousands_delimiter);
    const std::string* actual_decimal_delimiter =
        settings->FindStringKey("decimalDelimiter");
    ASSERT_TRUE(actual_decimal_delimiter);
    EXPECT_EQ(decimal_delimiter, *actual_decimal_delimiter);

    ASSERT_TRUE(
        settings->FindKeyOfType("unitType", base::Value::Type::INTEGER));
    ASSERT_TRUE(settings->FindKeyOfType("previewModifiable",
                                        base::Value::Type::BOOLEAN));
    const base::Value* title =
        settings->FindKeyOfType("documentTitle", base::Value::Type::STRING);
    ASSERT_TRUE(title);
    EXPECT_EQ(initiator_title, title->GetString());
    ASSERT_TRUE(settings->FindKeyOfType("documentHasSelection",
                                        base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindKeyOfType("shouldPrintSelectionOnly",
                                        base::Value::Type::BOOLEAN));
    const base::Value* printer =
        settings->FindKeyOfType("printerName", base::Value::Type::STRING);
    ASSERT_TRUE(printer);
    EXPECT_EQ(default_printer_name, printer->GetString());

    ASSERT_TRUE(settings->FindKeyOfType("pdfPrinterDisabled",
                                        base::Value::Type::BOOLEAN));
    ASSERT_TRUE(settings->FindKeyOfType("destinationsManaged",
                                        base::Value::Type::BOOLEAN));
    ASSERT_TRUE(
        settings->FindKeyOfType("cloudPrintURL", base::Value::Type::STRING));
  }

  // Returns |policy_name| entry from initial settings policies.
  const base::Value* GetInitialSettingsPolicy(const base::Value& settings,
                                              const std::string& policy_name) {
    const base::Value* policies =
        settings.FindKeyOfType("policies", base::Value::Type::DICTIONARY);
    if (!policies)
      return nullptr;
    return policies->FindKeyOfType(policy_name, base::Value::Type::DICTIONARY);
  }

  // Validates the initial settings value policies structure in the response
  // matches the print_preview.Policies type in
  // chrome/browser/resources/print_preview/native_layer.js.
  // Assumes "test-callback-id-0" was used as the callback id.
  void ValidateInitialSettingsValuePolicy(
      const content::TestWebUI::CallData& data,
      const std::string& policy_name,
      base::Optional<base::Value> expected_policy_value) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value* settings = data.arg3();

    const base::Value* policy =
        GetInitialSettingsPolicy(*settings, policy_name);
    const base::Value* policy_value =
        policy ? policy->FindKey("value") : nullptr;

    ASSERT_EQ(expected_policy_value.has_value(), !!policy_value);
    if (expected_policy_value.has_value())
      EXPECT_EQ(expected_policy_value.value(), *policy_value);
  }

  // Validates the initial settings allowed/default mode policies structure in
  // the response matches the print_preview.Policies type in
  // chrome/browser/resources/print_preview/native_layer.js.
  // Assumes "test-callback-id-0" was used as the callback id.
  void ValidateInitialSettingsAllowedDefaultModePolicy(
      const content::TestWebUI::CallData& data,
      const std::string& policy_name,
      base::Optional<base::Value> expected_allowed_mode,
      base::Optional<base::Value> expected_default_mode) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value* settings = data.arg3();

    const base::Value* policy =
        GetInitialSettingsPolicy(*settings, policy_name);
    const base::Value* allowed_mode =
        policy ? policy->FindKey("allowedMode") : nullptr;
    const base::Value* default_mode =
        policy ? policy->FindKey("defaultMode") : nullptr;

    ASSERT_EQ(expected_allowed_mode.has_value(), !!allowed_mode);
    if (expected_allowed_mode.has_value())
      EXPECT_EQ(expected_allowed_mode.value(), *allowed_mode);

    ASSERT_EQ(expected_default_mode.has_value(), !!default_mode);
    if (expected_default_mode.has_value())
      EXPECT_EQ(expected_default_mode.value(), *default_mode);
  }

  // Simulates a 'getPrinters' Web UI message by constructing the arguments and
  // making the call to the handler.
  void SendGetPrinters(PrinterType type, const std::string& callback_id_in) {
    base::Value args(base::Value::Type::LIST);
    args.Append(callback_id_in);
    args.Append(static_cast<int>(type));
    handler()->HandleGetPrinters(&base::Value::AsListValue(args));
  }

  // Validates that the printers-added Web UI event has been fired for
  // |expected-type| with 1 printer. This should be the second most recent call,
  // as the resolution of the getPrinters() promise will be the most recent.
  void ValidatePrinterTypeAdded(PrinterType expected_type) {
    const size_t call_data_size = web_ui()->call_data().size();
    ASSERT_GE(call_data_size, 2u);
    const content::TestWebUI::CallData& add_data =
        *web_ui()->call_data()[call_data_size - 2];
    AssertWebUIEventFired(add_data, "printers-added");
    const auto type = static_cast<PrinterType>(add_data.arg2()->GetInt());
    EXPECT_EQ(expected_type, type);
    ASSERT_TRUE(add_data.arg3());
    base::Value::ConstListView printer_list = add_data.arg3()->GetList();
    ASSERT_EQ(printer_list.size(), 1u);
    EXPECT_TRUE(printer_list[0].FindKeyOfType("printer_name",
                                              base::Value::Type::STRING));
  }

  // Simulates a 'getPrinterCapabilities' Web UI message by constructing the
  // arguments and making the call to the handler.
  void SendGetPrinterCapabilities(PrinterType type,
                                  const std::string& callback_id_in,
                                  const std::string& printer_name) {
    base::Value args(base::Value::Type::LIST);
    args.Append(callback_id_in);
    args.Append(printer_name);
    args.Append(static_cast<int>(type));
    handler()->HandleGetPrinterCapabilities(&base::Value::AsListValue(args));
  }

  // Validates that a printer capabilities promise was resolved/rejected.
  void ValidatePrinterCapabilities(const std::string& callback_id_in,
                                   bool expect_resolved) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, expect_resolved);
    if (expect_resolved) {
      const base::Value* settings = data.arg3();
      ASSERT_TRUE(settings);
      EXPECT_TRUE(settings->FindKeyOfType(kSettingCapabilities,
                                          base::Value::Type::DICTIONARY));
    }
  }

  blink::AssociatedInterfaceProvider*
  GetInitiatorAssociatedInterfaceProvider() {
    return initiator_web_contents_->GetMainFrame()
        ->GetRemoteAssociatedInterfaces();
  }

  const Profile* profile() { return profile_.get(); }
  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_->GetTestingPrefService();
  }
  content::TestWebUI* web_ui() { return web_ui_.get(); }
  TestPrintPreviewHandler* handler() { return handler_; }
  TestPrinterHandler* printer_handler() { return printer_handler_; }
  std::vector<PrinterInfo>& printers() { return printers_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::WebContents> initiator_web_contents_;
  std::vector<PrinterInfo> printers_;
  TestPrinterHandler* printer_handler_;
  TestPrintPreviewHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandlerTest);
};

TEST_F(PrintPreviewHandlerTest, InitialSettingsSimple) {
  Initialize();

  // Verify initial settings were sent.
  ValidateInitialSettings(*web_ui()->call_data().back(), kDummyPrinterName,
                          kDummyInitiatorName);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsHiLocale) {
  InitializeWithLocale("hi");

  // Verify initial settings were sent for Hindi.
  ValidateInitialSettingsForLocale(*web_ui()->call_data().back(),
                                   kDummyPrinterName, kDummyInitiatorName, "hi",
                                   ",", ".");
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRuLocale) {
  InitializeWithLocale("ru");

  // Verify initial settings were sent for Russian.
  ValidateInitialSettingsForLocale(*web_ui()->call_data().back(),
                                   kDummyPrinterName, kDummyInitiatorName, "ru",
                                   "\xC2\xA0", ",");
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsNoPolicies) {
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", base::nullopt,
                                                  base::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground",
                                                  base::nullopt, base::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", base::nullopt, base::nullopt);
  ValidateInitialSettingsValuePolicy(*web_ui()->call_data().back(), "sheets",
                                     base::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRestrictHeaderFooterEnabled) {
  // Set a pref with allowed value.
  prefs()->SetManagedPref(prefs::kPrintHeaderFooter,
                          std::make_unique<base::Value>(true));
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "headerFooter", base::Value(true),
      base::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRestrictHeaderFooterDisabled) {
  // Set a pref with allowed value.
  prefs()->SetManagedPref(prefs::kPrintHeaderFooter,
                          std::make_unique<base::Value>(false));
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "headerFooter", base::Value(false),
      base::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableHeaderFooter) {
  // Set a pref that should take priority over StickySettings.
  prefs()->SetBoolean(prefs::kPrintHeaderFooter, true);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", base::nullopt,
                                                  base::Value(true));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDisableHeaderFooter) {
  // Set a pref that should take priority over StickySettings.
  prefs()->SetBoolean(prefs::kPrintHeaderFooter, false);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", base::nullopt,
                                                  base::Value(false));
}

TEST_F(PrintPreviewHandlerTest,
       InitialSettingsRestrictBackgroundGraphicsEnabled) {
  // Set a pref with allowed value.
  prefs()->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 1);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "cssBackground", base::Value(1),
      base::nullopt);
}

TEST_F(PrintPreviewHandlerTest,
       InitialSettingsRestrictBackgroundGraphicsDisabled) {
  // Set a pref with allowed value.
  prefs()->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 2);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "cssBackground", base::Value(2),
      base::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableBackgroundGraphics) {
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 1);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "cssBackground", base::nullopt,
      base::Value(1));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDisableBackgroundGraphics) {
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 2);
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "cssBackground", base::nullopt,
      base::Value(2));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultPaperSizeName) {
  const char kPrintingPaperSizeDefaultName[] = R"(
    {
      "name": "iso_a5_148x210mm"
    })";
  const char kExpectedInitialSettingsPolicy[] = R"(
    {
      "width": 148000,
      "height": 210000
    })";

  base::Optional<base::Value> default_paper_size =
      base::JSONReader::Read(kPrintingPaperSizeDefaultName);
  ASSERT_TRUE(default_paper_size.has_value());
  // Set a pref that should take priority over StickySettings.
  prefs()->Set(prefs::kPrintingPaperSizeDefault, default_paper_size.value());
  Initialize();

  base::Optional<base::Value> expected_initial_settings_policy =
      base::JSONReader::Read(kExpectedInitialSettingsPolicy);
  ASSERT_TRUE(expected_initial_settings_policy.has_value());

  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", base::nullopt,
      std::move(expected_initial_settings_policy));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultPaperSizeCustomSize) {
  const char kPrintingPaperSizeDefaultCustomSize[] = R"(
    {
      "name": "custom",
      "custom_size": {
        "width": 148000,
        "height": 210000
      }
    })";
  const char kExpectedInitialSettingsPolicy[] = R"(
    {
      "width": 148000,
      "height": 210000
    })";

  base::Optional<base::Value> default_paper_size =
      base::JSONReader::Read(kPrintingPaperSizeDefaultCustomSize);
  ASSERT_TRUE(default_paper_size.has_value());
  // Set a pref that should take priority over StickySettings.
  prefs()->Set(prefs::kPrintingPaperSizeDefault, default_paper_size.value());
  Initialize();

  base::Optional<base::Value> expected_initial_settings_policy =
      base::JSONReader::Read(kExpectedInitialSettingsPolicy);
  ASSERT_TRUE(expected_initial_settings_policy.has_value());

  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", base::nullopt,
      std::move(expected_initial_settings_policy));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(PrintPreviewHandlerTest, InitialSettingsMaxSheetsAllowedPolicy) {
  prefs()->SetInteger(prefs::kPrintingMaxSheetsAllowed, 2);
  Initialize();
  ValidateInitialSettingsValuePolicy(*web_ui()->call_data().back(), "sheets",
                                     base::Value(2));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(PrintPreviewHandlerTest, GetPrinters) {
  Initialize();

  // Check all three printer types that implement
  for (size_t i = 0; i < base::size(kFetchableTypes); i++) {
    PrinterType type = kFetchableTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinters(type, callback_id_in);

    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 2 more for each loop
    // iteration (one for printers-added, and one for the response).
    ASSERT_EQ(1u + 2 * (i + 1), web_ui()->call_data().size());

    ValidatePrinterTypeAdded(type);

    // Verify getPrinters promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, true);
  }
}

// Validates the 'printing.printer_type_deny_list' pref by placing the extension
// and privet printer types on a deny list. A 'getPrinters' Web UI message is
// then called for all three fetchable printer types; only local printers should
// be successfully fetched.
TEST_F(PrintPreviewHandlerTest, GetNoDenyListPrinters) {
  base::Value::ListStorage deny_list;
  deny_list.push_back(base::Value("extension"));
  deny_list.push_back(base::Value("privet"));
  prefs()->Set(prefs::kPrinterTypeDenyList, base::Value(std::move(deny_list)));
  Initialize();

  size_t expected_callbacks = 1;
  for (size_t i = 0; i < base::size(kFetchableTypes); i++) {
    PrinterType type = kFetchableTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinters(type, callback_id_in);

    // Start with 1 call from initial settings, then add 2 more for each printer
    // type that isn't on the deny list (one for printers-added, and one for the
    // response), and only 1 more for each type on the deny list (just for
    // response).
    const bool is_allowed_type = type == PrinterType::kLocal;
    EXPECT_EQ(is_allowed_type, handler()->CalledOnlyForType(type));
    expected_callbacks += is_allowed_type ? 2 : 1;
    ASSERT_EQ(expected_callbacks, web_ui()->call_data().size());

    if (is_allowed_type) {
      ValidatePrinterTypeAdded(type);
    }

    // Verify getPrinters promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, true);
  }
}

TEST_F(PrintPreviewHandlerTest, GetPrinterCapabilities) {
  // Add an empty printer to the handler.
  printers().push_back(GetEmptyPrinterInfo());
  printer_handler()->SetPrinters(printers());

  // Initial settings first to enable javascript.
  Initialize();

  // Check all four printer types that implement
  // PrinterHandler::StartGetCapability().
  for (size_t i = 0; i < base::size(kAllSupportedTypes); i++) {
    PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, kDummyPrinterName);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(1u + (i + 1), web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, /*expect_resolved=*/true);
  }

  // Run through the loop again, this time with a printer that has no
  // capabilities.
  for (size_t i = 0; i < base::size(kAllSupportedTypes); i++) {
    PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" +
        base::NumberToString(i + base::size(kAllSupportedTypes) + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, kEmptyPrinterName);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings plus
    // base::size(kAllSupportedTypes) from first loop, then add 1 more for each
    // loop iteration.
    ASSERT_EQ(1u + base::size(kAllSupportedTypes) + (i + 1),
              web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, /*expect_resolved=*/false);
  }
}

// Validates the 'printing.printer_type_deny_list' pref by placing the local and
// PDF printer types on the deny list. A 'getPrinterCapabilities' Web UI message
// is then called for all supported printer types; only privet and extension
// printer capabilties should be successfully fetched.
TEST_F(PrintPreviewHandlerTest, GetNoDenyListPrinterCapabilities) {
  base::Value::ListStorage deny_list;
  deny_list.push_back(base::Value("local"));
  deny_list.push_back(base::Value("pdf"));
  prefs()->Set(prefs::kPrinterTypeDenyList, base::Value(std::move(deny_list)));
  Initialize();

  // Check all four printer types that implement
  // PrinterHandler::StartGetCapability().
  for (size_t i = 0; i < base::size(kAllSupportedTypes); i++) {
    PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, kDummyPrinterName);

    const bool is_allowed_type =
        type == PrinterType::kPrivet || type == PrinterType::kExtension;
    EXPECT_EQ(is_allowed_type, handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(1u + (i + 1), web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, is_allowed_type);
  }
}

TEST_F(PrintPreviewHandlerTest, Print) {
  Initialize();

  // All printer types can print.
  for (size_t i = 0; i < base::size(kAllTypes); i++) {
    base::HistogramTester histograms;
    handler()->reset_calls();

    // Send print preview request.
    base::Value preview_ticket = GetPrintPreviewTicket();
    preview_ticket.SetIntKey(kPreviewRequestID, i);
    std::string preview_callback_id =
        "test-callback-id-" + base::NumberToString(2 * i + 1);
    std::unique_ptr<base::ListValue> preview_list_args =
        ConstructPreviewArgs(preview_callback_id, preview_ticket);
    handler()->HandleGetPreview(preview_list_args.get());

    // Send printing request.
    PrinterType type = kAllTypes[i];
    base::Value print_args(base::Value::Type::LIST);
    std::string print_callback_id =
        "test-callback-id-" + base::NumberToString(2 * (i + 1));
    print_args.Append(print_callback_id);
    base::Value print_ticket = GetPrintTicket(type);
    std::string json;
    base::JSONWriter::Write(print_ticket, &json);
    print_args.Append(json);
    std::unique_ptr<base::ListValue> print_list_args = base::ListValue::From(
        base::Value::ToUniquePtrValue(std::move(print_args)));
    handler()->HandlePrint(print_list_args.get());

    CheckHistograms(histograms, type);

    // Verify correct PrinterHandler was called or that no handler was requested
    // for cloud printers.
    if (type == PrinterType::kCloud) {
      EXPECT_TRUE(handler()->NotCalled());
    } else {
      EXPECT_TRUE(handler()->CalledOnlyForType(type));
    }

    // Verify print promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, print_callback_id, true);

    // For cloud print, should also get the encoded data back as a string.
    if (type == PrinterType::kCloud) {
      std::string print_data;
      ASSERT_TRUE(data.arg3()->GetAsString(&print_data));
      std::string expected_data;
      base::Base64Encode(kTestData, &expected_data);
      EXPECT_EQ(print_data, expected_data);
    }
  }
}

TEST_F(PrintPreviewHandlerTest, GetPreview) {
  Initialize();

  base::RunLoop run_loop;
  TestPrintPreviewPrintRenderFrame print_render_frame(
      GetInitiatorAssociatedInterfaceProvider());
  print_render_frame.SetCompletionClosure(run_loop.QuitClosure());

  base::Value print_ticket = GetPrintPreviewTicket();
  std::unique_ptr<base::ListValue> list_args =
      ConstructPreviewArgs("test-callback-id-1", print_ticket);
  handler()->HandleGetPreview(list_args.get());
  run_loop.Run();

  // Verify that the preview was requested from the renderer with the
  // appropriate settings.
  const base::Value& preview_params = print_render_frame.GetSettings();
  bool preview_id_found = false;
  for (const auto& it : preview_params.DictItems()) {
    if (it.first == kPreviewUIID) {  // This is added by the handler.
      preview_id_found = true;
      continue;
    }
    const base::Value* value_in = print_ticket.FindKey(it.first);
    ASSERT_TRUE(value_in);
    EXPECT_EQ(*value_in, it.second);
  }
  EXPECT_TRUE(preview_id_found);
}

TEST_F(PrintPreviewHandlerTest, SendPreviewUpdates) {
  Initialize();

  base::RunLoop run_loop;
  TestPrintPreviewPrintRenderFrame print_render_frame(
      GetInitiatorAssociatedInterfaceProvider());
  print_render_frame.SetCompletionClosure(run_loop.QuitClosure());

  const char callback_id_in[] = "test-callback-id-1";
  base::Value print_ticket = GetPrintPreviewTicket();
  std::unique_ptr<base::ListValue> list_args =
      ConstructPreviewArgs(callback_id_in, print_ticket);
  handler()->HandleGetPreview(list_args.get());
  run_loop.Run();
  const base::Value& preview_params = print_render_frame.GetSettings();

  // Read the preview UI ID and request ID
  base::Optional<int> request_value =
      preview_params.FindIntKey(kPreviewRequestID);
  ASSERT_TRUE(request_value.has_value());
  int preview_request_id = request_value.value();

  base::Optional<int> ui_value = preview_params.FindIntKey(kPreviewUIID);
  ASSERT_TRUE(ui_value.has_value());
  int preview_ui_id = ui_value.value();

  // Simulate renderer responses: PageLayoutReady, PageCountReady,
  // PagePreviewReady, and OnPrintPreviewReady will be called in that order.
  base::Value layout(base::Value::Type::DICTIONARY);
  layout.SetDoubleKey(kSettingMarginTop, 34.0);
  layout.SetDoubleKey(kSettingMarginLeft, 34.0);
  layout.SetDoubleKey(kSettingMarginBottom, 34.0);
  layout.SetDoubleKey(kSettingMarginRight, 34.0);
  layout.SetDoubleKey(kSettingContentWidth, 544.0);
  layout.SetDoubleKey(kSettingContentHeight, 700.0);
  layout.SetIntKey(kSettingPrintableAreaX, 17);
  layout.SetIntKey(kSettingPrintableAreaY, 17);
  layout.SetIntKey(kSettingPrintableAreaWidth, 578);
  layout.SetIntKey(kSettingPrintableAreaHeight, 734);
  handler()->SendPageLayoutReady(base::Value::AsDictionaryValue(layout),
                                 /*has_custom_page_size_style,=*/false,
                                 preview_request_id);

  // Verify that page-layout-ready webUI event was fired.
  AssertWebUIEventFired(*web_ui()->call_data().back(), "page-layout-ready");

  // 1 page document. Modifiable so send default 100 scaling.
  handler()->SendPageCountReady(1, 100, preview_request_id);
  AssertWebUIEventFired(*web_ui()->call_data().back(), "page-count-ready");

  // Page at index 0 is ready.
  handler()->SendPagePreviewReady(0, preview_ui_id, preview_request_id);
  AssertWebUIEventFired(*web_ui()->call_data().back(), "page-preview-ready");

  // Print preview is ready.
  handler()->OnPrintPreviewReady(preview_ui_id, preview_request_id);
  CheckWebUIResponse(*web_ui()->call_data().back(), callback_id_in, true);

  // Renderer responses have been as expected.
  EXPECT_EQ(handler()->bad_messages(), 0);

  // None of these should work since there has been no new preview request.
  // Check that there are no new web UI messages sent.
  size_t message_count = web_ui()->call_data().size();
  handler()->SendPageLayoutReady(base::DictionaryValue(),
                                 /*has_custom_page_size_style,=*/false,
                                 preview_request_id);
  EXPECT_EQ(message_count, web_ui()->call_data().size());
  handler()->SendPageCountReady(1, -1, 0);
  EXPECT_EQ(message_count, web_ui()->call_data().size());
  handler()->OnPrintPreviewReady(0, 0);
  EXPECT_EQ(message_count, web_ui()->call_data().size());

  // Handler should have tried to kill the renderer for each of these.
  EXPECT_EQ(handler()->bad_messages(), 3);
}

class FailingTestPrinterHandler : public TestPrinterHandler {
 public:
  explicit FailingTestPrinterHandler(const std::vector<PrinterInfo>& printers)
      : TestPrinterHandler(printers) {}

  ~FailingTestPrinterHandler() override = default;

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {
    std::move(callback).Run(base::Value());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FailingTestPrinterHandler);
};

class PrintPreviewHandlerFailingTest : public PrintPreviewHandlerTest {
 public:
  PrintPreviewHandlerFailingTest() = default;
  ~PrintPreviewHandlerFailingTest() override = default;

  std::unique_ptr<TestPrinterHandler> CreatePrinterHandler(
      const std::vector<PrinterInfo>& printers) override {
    return std::make_unique<FailingTestPrinterHandler>(printers);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandlerFailingTest);
};

// This test is similar to PrintPreviewHandlerTest.GetPrinterCapabilities, but
// uses FailingTestPrinterHandler instead of TestPrinterHandler. As a result,
// StartGetCapability() always fails, to exercise its callback's failure
// handling path. Failure is different from getting no capabilities.
TEST_F(PrintPreviewHandlerFailingTest, GetPrinterCapabilities) {
  // Add an empty printer to the handler.
  printers().push_back(GetEmptyPrinterInfo());
  printer_handler()->SetPrinters(printers());

  // Initial settings first to enable javascript.
  Initialize();

  // Check all four printer types that implement
  // PrinterHandler::StartGetCapability().
  for (size_t i = 0; i < base::size(kAllSupportedTypes); i++) {
    PrinterType type = kAllSupportedTypes[i];
    handler()->reset_calls();
    base::Value args(base::Value::Type::LIST);
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    args.Append(callback_id_in);
    args.Append(kDummyPrinterName);
    args.Append(static_cast<int>(type));
    std::unique_ptr<base::ListValue> list_args =
        base::ListValue::From(base::Value::ToUniquePtrValue(std::move(args)));
    handler()->HandleGetPrinterCapabilities(list_args.get());
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(1u + (i + 1), web_ui()->call_data().size());

    // Verify printer capabilities promise was rejected.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, false);
    const base::Value* settings = data.arg3();
    ASSERT_TRUE(settings);
    EXPECT_TRUE(settings->is_none());
  }
}

}  // namespace printing
