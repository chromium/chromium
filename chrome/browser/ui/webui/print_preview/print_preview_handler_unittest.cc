// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/i18n/number_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/webui/print_preview/fake_print_render_frame.h"
#include "chrome/browser/ui/webui/print_preview/policy_settings.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_metrics.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "printing/buildflags/buildflags.h"
#include "printing/mojom/print.mojom.h"
#include "printing/printing_features.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/test/deep_scanning_test_utils.h"
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_delegate.h"
#include "chrome/browser/policy/dm_token_utils.h"

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/test/fake_content_analysis_sdk_manager.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
#include "chrome/test/base/testing_profile_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ash/crosapi/test_local_printer_ash.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#endif

namespace printing {

namespace {

const char kDummyInitiatorName[] = "TestInitiator";
const char16_t kDummyInitiatorName16[] = u"TestInitiator";
const char kEmptyPrinterName[] = "EmptyPrinter";
const char kTestData[] = "abc";

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
constexpr char kFakeDmToken[] = "fake-dm-token";
constexpr char kCallbackId[] = "test-callback-id-1";
#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

// Array of all mojom::PrinterTypes.
constexpr mojom::PrinterType kAllTypes[] = {mojom::PrinterType::kExtension,
                                            mojom::PrinterType::kPdf,
                                            mojom::PrinterType::kLocal};

// Array of all mojom::PrinterTypes that have working PrinterHandlers.
constexpr mojom::PrinterType kAllSupportedTypes[] = {
    mojom::PrinterType::kExtension, mojom::PrinterType::kPdf,
    mojom::PrinterType::kLocal};

// Both printer types that implement PrinterHandler::StartGetPrinters().
constexpr mojom::PrinterType kFetchableTypes[] = {
    mojom::PrinterType::kExtension, mojom::PrinterType::kLocal};

struct PrinterInfo {
  std::string id;
  bool is_default;
  base::Value::Dict basic_info;
  base::Value::Dict capabilities;
};

PrinterInfo GetSimplePrinterInfo(const std::string& name, bool is_default) {
  PrinterInfo simple_printer;
  simple_printer.id = name;
  simple_printer.is_default = is_default;
  simple_printer.basic_info.Set("printer_name", simple_printer.id);
  simple_printer.basic_info.Set("printer_description", "Printer for test");
  simple_printer.basic_info.Set("printer_status", 1);
  base::Value::Dict cdd;
  base::Value::Dict capabilities;
  simple_printer.capabilities.Set("printer", simple_printer.basic_info.Clone());
  simple_printer.capabilities.Set("capabilities", cdd.Clone());
  return simple_printer;
}

PrinterInfo GetEmptyPrinterInfo() {
  PrinterInfo empty_printer;
  empty_printer.id = kEmptyPrinterName;
  empty_printer.is_default = false;
  empty_printer.basic_info.Set("printer_name", empty_printer.id);
  empty_printer.basic_info.Set("printer_description",
                               "Printer with no capabilities");
  empty_printer.basic_info.Set("printer_status", 0);
  empty_printer.capabilities.Set("printer", empty_printer.basic_info.Clone());
  return empty_printer;
}

base::Value::Dict GetPrintPreviewTicket() {
  base::Value::Dict print_ticket =
      test::GetPrintTicket(mojom::PrinterType::kLocal);

  // Make some modifications to match a preview print ticket.
  print_ticket.Set(kSettingPageRange, base::Value());
  print_ticket.Set(kIsFirstRequest, true);
  print_ticket.Set(kPreviewRequestID, 0);
  print_ticket.Set(kSettingPreviewModifiable, false);
  print_ticket.Remove(kSettingPageWidth);
  print_ticket.Remove(kSettingPageHeight);
  print_ticket.Remove(kSettingShowSystemDialog);

  return print_ticket;
}

base::Value::List ConstructPreviewArgs(std::string_view callback_id,
                                       const base::Value::Dict& print_ticket) {
  base::Value::List args;
  args.Append(callback_id);
  std::string json;
  base::JSONWriter::Write(base::Value(print_ticket.Clone()), &json);
  args.Append(json);
  return args;
}

UserActionBuckets GetUserActionForPrinterType(mojom::PrinterType type) {
  switch (type) {
    case mojom::PrinterType::kPrivetDeprecated:
      NOTREACHED_IN_MIGRATION();
      // Return value doesn't matter.
      return UserActionBuckets::kPrintToPrinter;
    case mojom::PrinterType::kExtension:
      return UserActionBuckets::kPrintWithExtension;
    case mojom::PrinterType::kPdf:
      return UserActionBuckets::kPrintToPdf;
    case mojom::PrinterType::kLocal:
      return UserActionBuckets::kPrintToPrinter;
    case mojom::PrinterType::kCloudDeprecated:
      NOTREACHED_IN_MIGRATION();
      // Return value doesn't matter.
      return UserActionBuckets::kPrintWithCloudPrint;
  }
}

// Checks whether |histograms| was updated correctly by a job with a printer
// type |type| with arguments generated by GetPrintTicket().
void CheckHistograms(const base::HistogramTester& histograms,
                     mojom::PrinterType type) {
  static constexpr PrintSettingsBuckets kPopulatedPrintSettingsBuckets[] = {
      PrintSettingsBuckets::kPortrait, PrintSettingsBuckets::kColor,
      PrintSettingsBuckets::kCollate,  PrintSettingsBuckets::kDuplex,
      PrintSettingsBuckets::kTotal,    PrintSettingsBuckets::kDefaultMedia,
  };

  for (auto bucket : kPopulatedPrintSettingsBuckets)
    histograms.ExpectBucketCount("PrintPreview.PrintSettings", bucket, 1);

  // All other PrintPreview.PrintSettings buckets should be empty.
  histograms.ExpectTotalCount("PrintPreview.PrintSettings",
                              std::size(kPopulatedPrintSettingsBuckets));

  const UserActionBuckets user_action = GetUserActionForPrinterType(type);
  histograms.ExpectBucketCount("PrintPreview.UserAction", user_action, 1);
  // Only one PrintPreview.UserAction bucket should have been populated.
  histograms.ExpectTotalCount("PrintPreview.UserAction", 1);

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
  TestPrinterHandler(const TestPrinterHandler&) = delete;
  TestPrinterHandler& operator=(const TestPrinterHandler&) = delete;
  ~TestPrinterHandler() override = default;

  void Reset() override {}

  void GetDefaultPrinter(DefaultPrinterCallback cb) override {
    std::move(cb).Run(default_printer_);
  }

  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override {
    if (!printers_.empty())
      added_printers_callback.Run(printers_.Clone());
    std::move(done_callback).Run();
  }

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {
    std::move(callback).Run(printer_capabilities_[destination_id].Clone());
  }

  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override {}

  void StartPrint(const std::u16string& job_title,
                  base::Value::Dict settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override {
    std::move(callback).Run(base::Value());
  }

  void SetPrinters(const std::vector<PrinterInfo>& printers) {
    printers_.clear();
    for (const auto& printer : printers) {
      if (printer.is_default)
        default_printer_ = printer.id;
      printers_.Append(printer.basic_info.Clone());
      printer_capabilities_[printer.id] = printer.capabilities.Clone();
    }
  }

 private:
  std::string default_printer_;
  base::Value::List printers_;
  std::map<std::string, base::Value::Dict> printer_capabilities_;
};

class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  FakePrintPreviewUI(content::WebUI* web_ui,
                     std::unique_ptr<PrintPreviewHandler> handler)
      : PrintPreviewUI(web_ui, std::move(handler)) {}
  FakePrintPreviewUI(const FakePrintPreviewUI&) = delete;
  FakePrintPreviewUI& operator=(const FakePrintPreviewUI&) = delete;
  ~FakePrintPreviewUI() override = default;

  void GetPrintPreviewDataForIndex(
      int index,
      scoped_refptr<base::RefCountedMemory>* data) const override {
    *data = base::MakeRefCounted<base::RefCountedStaticMemory>(
        base::byte_span_from_cstring(kTestData));
  }

  void OnPrintPreviewRequest(int request_id) override {}
  void OnCancelPendingPreviewRequest() override {}
  void OnHidePreviewDialog() override {}
  void OnClosePrintPreviewDialog() override {}
};

class TestPrintPreviewPrintRenderFrame final : public FakePrintRenderFrame {
 public:
  explicit TestPrintPreviewPrintRenderFrame(
      blink::AssociatedInterfaceProvider* provider)
      : FakePrintRenderFrame(provider) {}
  TestPrintPreviewPrintRenderFrame(const TestPrintPreviewPrintRenderFrame&) =
      delete;
  TestPrintPreviewPrintRenderFrame& operator=(
      const TestPrintPreviewPrintRenderFrame&) = delete;
  ~TestPrintPreviewPrintRenderFrame() override = default;

  const base::Value::Dict& GetSettings() { return settings_; }

  void SetCompletionClosure(base::OnceClosure closure) {
    closure_ = std::move(closure);
  }

 private:
  // FakePrintRenderFrame:
  void PrintPreview(base::Value::Dict settings) override {
    settings_ = std::move(settings);
    std::move(closure_).Run();
  }

  base::OnceClosure closure_;
  base::Value::Dict settings_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
class TestLocalPrinter : public FakeLocalPrinter {
 public:
  TestLocalPrinter() : policies_(crosapi::mojom::Policies()) {}
  TestLocalPrinter(const TestLocalPrinter&) = delete;
  TestLocalPrinter& operator=(const TestLocalPrinter&) = delete;
  ~TestLocalPrinter() override = default;

  void set_policies(const crosapi::mojom::Policies& policies) {
    policies_ = policies;
  }

  void add_to_deny_list(const mojom::PrinterType& printer_type) {
    deny_list_.push_back(printer_type);
  }

  // crosapi::mojom::LocalPrinter:
  void GetPolicies(GetPoliciesCallback callback) override {
    ASSERT_TRUE(policies_);
    std::move(callback).Run(policies_->Clone());
    policies_.reset();
  }
  void GetPrinterTypeDenyList(
      GetPrinterTypeDenyListCallback callback) override {
    std::move(callback).Run(deny_list_);
  }

 private:
  std::optional<crosapi::mojom::Policies> policies_;
  std::vector<mojom::PrinterType> deny_list_;
};
#endif

class TestPrintPreviewHandler : public PrintPreviewHandler {
 public:
  TestPrintPreviewHandler(std::unique_ptr<PrinterHandler> printer_handler,
                          content::WebContents* initiator)
      : bad_messages_(0),
        test_printer_handler_(std::move(printer_handler)),
        initiator_(initiator) {}
  TestPrintPreviewHandler(const TestPrintPreviewHandler&) = delete;
  TestPrintPreviewHandler& operator=(const TestPrintPreviewHandler&) = delete;
  ~TestPrintPreviewHandler() override = default;

  PrinterHandler* GetPrinterHandler(mojom::PrinterType printer_type) override {
    called_for_type_.insert(printer_type);
    return test_printer_handler_.get();
  }

  void BadMessageReceived() override { bad_messages_++; }

  content::WebContents* GetInitiator() override { return initiator_; }

  bool CalledOnlyForType(mojom::PrinterType printer_type) {
    return (called_for_type_.size() == 1 &&
            *called_for_type_.begin() == printer_type);
  }

  bool NotCalled() { return called_for_type_.empty(); }

  void reset_calls() { called_for_type_.clear(); }

  int bad_messages() { return bad_messages_; }

 private:
  int bad_messages_;
  base::flat_set<mojom::PrinterType> called_for_type_;
  std::unique_ptr<PrinterHandler> test_printer_handler_;
  const raw_ptr<content::WebContents, DanglingUntriaged> initiator_;
};

#if BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
class TestPrintPreviewHandlerForContentAnalysis
    : public TestPrintPreviewHandler {
 public:
  TestPrintPreviewHandlerForContentAnalysis(
      std::unique_ptr<PrinterHandler> printer_handler,
      content::WebContents* web_contents)
      : TestPrintPreviewHandler(std::move(printer_handler), web_contents) {}

  ~TestPrintPreviewHandlerForContentAnalysis() override = default;

  bool print_called_after_scan() const { return print_called_after_scan_; }

 private:
  void FinishHandleDoPrint(UserActionBuckets user_action,
                           base::Value::Dict settings,
                           scoped_refptr<base::RefCountedMemory> data,
                           const std::string& callback_id) override {
    ASSERT_EQ(base::as_string_view(*data), kTestData);
    print_called_after_scan_ = true;
    PrintPreviewHandler::FinishHandleDoPrint(user_action, std::move(settings),
                                             data, callback_id);
  }

  bool print_called_after_scan_ = false;
};
#endif  // BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)

}  // namespace

class PrintPreviewHandlerTest : public testing::Test {
 public:
  PrintPreviewHandlerTest() = default;
  PrintPreviewHandlerTest(const PrintPreviewHandlerTest&) = delete;
  PrintPreviewHandlerTest& operator=(const PrintPreviewHandlerTest&) = delete;
  ~PrintPreviewHandlerTest() override = default;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void SetPolicies(const crosapi::mojom::Policies& policies) {
    local_printer_.set_policies(policies);
  }

  void AddToDenyList(const mojom::PrinterType& printer_type) {
    local_printer_.add_to_deny_list(printer_type);
  }
#endif

  void SetProfileForInitialSettings(TestingProfile* profile) {
    auto* dialog_controller = PrintPreviewDialogController::GetInstance();
    CHECK(dialog_controller);
    if (preview_web_contents_) {
      dialog_controller->DisassociateWebContentsesForTesting(
          preview_web_contents_.get());
    }

    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile));
    web_ui_->set_web_contents(preview_web_contents_.get());
    dialog_controller->AssociateWebContentsesForTesting(
        initiator_web_contents_.get(), preview_web_contents_.get());
  }

  void SetUp() override {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
    ASSERT_TRUE(testing_profile_manager_.SetUp());
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
    local_printer_ = std::make_unique<TestLocalPrinterAsh>(&profile_, nullptr);
    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
#endif

    // Create the initiator.
    initiator_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    content::WebContents* initiator = initiator_web_contents_.get();
    // Ensure the initiator has a RenderFrameHost with a live RenderFrame, as
    // the print code will not bother to send IPCs to a non-live RenderFrame.
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL("about:blank"), initiator->GetPrimaryMainFrame());

    // Create the print preview.
    web_ui_ = std::make_unique<content::TestWebUI>();
    SetProfileForInitialSettings(&profile_);

    // Create the PrintViewManager and put it to work.
    PrintViewManager::CreateForWebContents(initiator);
    PrintViewManager::FromWebContents(initiator)->PrintPreviewNow(
        initiator->GetPrimaryMainFrame(), false);

    printers_.push_back(GetSimplePrinterInfo(test::kPrinterName, true));
    auto printer_handler = CreatePrinterHandler(printers_);
    printer_handler_ = printer_handler.get();

    auto preview_handler = CreateHandler(std::move(printer_handler), initiator);
    handler_ = preview_handler.get();
    handler_->set_web_ui(web_ui());
#if BUILDFLAG(IS_CHROMEOS_ASH)
    handler_->local_printer_ = local_printer_.get();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    handler_->local_printer_ = &local_printer_;
    handler_->local_printer_version_ = crosapi::mojom::LocalPrinter::Version_;
#endif

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::move(preview_handler));
    preview_ui->SetInitiatorTitle(kDummyInitiatorName16);
    web_ui()->SetController(std::move(preview_ui));
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    manager_.reset();
    ash::LoginState::Shutdown();
#endif
    PrintViewManager::FromWebContents(initiator_web_contents_.get())
        ->PrintPreviewDone();

    auto* dialog_controller = PrintPreviewDialogController::GetInstance();
    CHECK(dialog_controller);
    dialog_controller->DisassociateWebContentsesForTesting(
        preview_web_contents_.get());
  }

#if BUILDFLAG(IS_CHROMEOS)
  void DisableAshChrome() { handler_->local_printer_ = nullptr; }
#endif

  virtual std::unique_ptr<TestPrinterHandler> CreatePrinterHandler(
      const std::vector<PrinterInfo>& printers) {
    return std::make_unique<TestPrinterHandler>(printers);
  }

  virtual std::unique_ptr<TestPrintPreviewHandler> CreateHandler(
      std ::unique_ptr<TestPrinterHandler> printer_handler,
      content::WebContents* web_contents) {
    return std::make_unique<TestPrintPreviewHandler>(std::move(printer_handler),
                                                     web_contents);
  }

  void Initialize() { InitializeWithLocale("en"); }

  void InitializeWithLocale(const std::string& locale) {
    // Sending this message will enable javascript, so it must always be called
    // before any other messages are sent.
    base::Value::List args;
    args.Append("test-callback-id-0");

    auto* browser_process = TestingBrowserProcess::GetGlobal();
    std::string original_locale = browser_process->GetApplicationLocale();
    {
      // Set locale since the delimiters checked in VerifyInitialSettings()
      // depend on it. This has to be done in several ways to make various
      // locale code sync up correctly.
      browser_process->SetApplicationLocale(locale);
      base::test::ScopedRestoreICUDefaultLocale scoped_locale(locale);
      base::ResetFormattersForTesting();
      handler()->HandleGetInitialSettings(args);
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
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(event_id, data.arg1()->GetString());
  }

  void CheckWebUIResponse(const content::TestWebUI::CallData& data,
                          const std::string& callback_id_in,
                          bool expect_success) {
    EXPECT_EQ("cr.webUIResponse", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(callback_id_in, data.arg1()->GetString());
    ASSERT_TRUE(data.arg2()->is_bool());
    EXPECT_EQ(expect_success, data.arg2()->GetBool());
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
    const base::Value::Dict& settings = data.arg3()->GetDict();
    ASSERT_TRUE(settings.FindBool("isInKioskAutoPrintMode").has_value());
    ASSERT_TRUE(settings.FindBool("isInAppKioskMode").has_value());

    const std::string* actual_locale = settings.FindString("uiLocale");
    ASSERT_TRUE(actual_locale);
    EXPECT_EQ(locale, *actual_locale);
    const std::string* actual_thousands_delimiter =
        settings.FindString("thousandsDelimiter");
    ASSERT_TRUE(actual_thousands_delimiter);
    EXPECT_EQ(thousands_delimiter, *actual_thousands_delimiter);
    const std::string* actual_decimal_delimiter =
        settings.FindString("decimalDelimiter");
    ASSERT_TRUE(actual_decimal_delimiter);
    EXPECT_EQ(decimal_delimiter, *actual_decimal_delimiter);

    ASSERT_TRUE(settings.FindInt("unitType").has_value());
    ASSERT_TRUE(settings.FindBool("previewModifiable").has_value());
    const std::string* title = settings.FindString("documentTitle");
    ASSERT_TRUE(title);
    EXPECT_EQ(initiator_title, *title);
    ASSERT_TRUE(settings.FindBool("documentHasSelection").has_value());
    ASSERT_TRUE(settings.FindBool("shouldPrintSelectionOnly").has_value());
    const std::string* printer = settings.FindString("printerName");
    ASSERT_TRUE(printer);
    EXPECT_EQ(default_printer_name, *printer);

    ASSERT_TRUE(settings.FindBool("pdfPrinterDisabled").has_value());
    ASSERT_TRUE(settings.FindBool("destinationsManaged").has_value());
  }

  // Returns |policy_name| entry from initial settings policies.
  const base::Value::Dict* GetInitialSettingsPolicy(
      const base::Value::Dict& settings,
      const std::string& policy_name) {
    const base::Value::Dict* policies = settings.FindDict("policies");
    if (!policies)
      return nullptr;
    return policies->FindDict(policy_name);
  }

  // Validates the initial settings value policies structure in the response
  // matches the print_preview.Policies type in
  // chrome/browser/resources/print_preview/native_layer.js.
  // Assumes "test-callback-id-0" was used as the callback id.
  void ValidateInitialSettingsValuePolicy(
      const content::TestWebUI::CallData& data,
      const std::string& policy_name,
      std::optional<base::Value> expected_policy_value) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value::Dict& settings = data.arg3()->GetDict();

    const base::Value::Dict* policy =
        GetInitialSettingsPolicy(settings, policy_name);
    const base::Value* policy_value = policy ? policy->Find("value") : nullptr;

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
      std::optional<base::Value> expected_allowed_mode,
      std::optional<base::Value> expected_default_mode) {
    CheckWebUIResponse(data, "test-callback-id-0", true);
    const base::Value::Dict& settings = data.arg3()->GetDict();

    const base::Value::Dict* policy =
        GetInitialSettingsPolicy(settings, policy_name);
    const base::Value* allowed_mode =
        policy ? policy->Find("allowedMode") : nullptr;
    const base::Value* default_mode =
        policy ? policy->Find("defaultMode") : nullptr;

    ASSERT_EQ(expected_allowed_mode.has_value(), !!allowed_mode);
    if (expected_allowed_mode.has_value())
      EXPECT_EQ(expected_allowed_mode.value(), *allowed_mode);

    ASSERT_EQ(expected_default_mode.has_value(), !!default_mode);
    if (expected_default_mode.has_value())
      EXPECT_EQ(expected_default_mode.value(), *default_mode);
  }

  // Simulates a 'getPrinters' Web UI message by constructing the arguments and
  // making the call to the handler.
  void SendGetPrinters(mojom::PrinterType type,
                       const std::string& callback_id_in) {
    base::Value::List args;
    args.Append(callback_id_in);
    args.Append(static_cast<int>(type));
    handler()->HandleGetPrinters(args);
  }

  // Validates that the printers-added Web UI event has been fired for
  // |expected-type| with 1 printer. This should be the second most recent call,
  // as the resolution of the getPrinters() promise will be the most recent.
  void ValidatePrinterTypeAdded(mojom::PrinterType expected_type) {
    const size_t call_data_size = web_ui()->call_data().size();
    ASSERT_GE(call_data_size, 2u);
    const content::TestWebUI::CallData& add_data =
        *web_ui()->call_data()[call_data_size - 2];
    AssertWebUIEventFired(add_data, "printers-added");
    const auto type =
        static_cast<mojom::PrinterType>(add_data.arg2()->GetInt());
    EXPECT_EQ(expected_type, type);
    ASSERT_TRUE(add_data.arg3());
    const base::Value::List& printer_list = add_data.arg3()->GetList();
    ASSERT_EQ(printer_list.size(), 1u);
    EXPECT_TRUE(printer_list[0].GetDict().FindString("printer_name"));
  }

  // Simulates a 'getPrinterCapabilities' Web UI message by constructing the
  // arguments and making the call to the handler.
  void SendGetPrinterCapabilities(mojom::PrinterType type,
                                  const std::string& callback_id_in,
                                  const std::string& printer_name) {
    base::Value::List args;
    args.Append(callback_id_in);
    args.Append(printer_name);
    args.Append(static_cast<int>(type));
    handler()->HandleGetPrinterCapabilities(args);
  }

  // Validates that a printer capabilities promise was resolved/rejected.
  void ValidatePrinterCapabilities(const std::string& callback_id_in,
                                   bool expect_resolved) {
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, callback_id_in, expect_resolved);
    if (expect_resolved) {
      ASSERT_TRUE(data.arg3());
      const base::Value::Dict& settings = data.arg3()->GetDict();
      EXPECT_TRUE(settings.FindDict(kSettingCapabilities));
    }
  }

  blink::AssociatedInterfaceProvider*
  GetInitiatorAssociatedInterfaceProvider() {
    return initiator_web_contents_->GetPrimaryMainFrame()
        ->GetRemoteAssociatedInterfaces();
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() {
    return profile_.GetTestingPrefService();
  }
  content::TestWebUI* web_ui() { return web_ui_.get(); }
  TestPrintPreviewHandler* handler() { return handler_; }
  TestingProfile* profile() { return &profile_; }
  TestPrinterHandler* printer_handler() { return printer_handler_; }
  std::vector<PrinterInfo>& printers() { return printers_; }
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  TestingProfileManager* testing_profile_manager() {
    return &testing_profile_manager_;
  }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(ENTERPRISE_CONTENT_ANALYSIS)
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestLocalPrinterAsh> local_printer_;
  std::unique_ptr<crosapi::CrosapiManager> manager_;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  TestLocalPrinter local_printer_;
#endif
  TestingProfile profile_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::WebContents> initiator_web_contents_;
  std::vector<PrinterInfo> printers_;
  raw_ptr<TestPrinterHandler> printer_handler_;
  raw_ptr<TestPrintPreviewHandler> handler_;
};

TEST_F(PrintPreviewHandlerTest, InitialSettingsSimple) {
  Initialize();

  // Verify initial settings were sent.
  ValidateInitialSettings(*web_ui()->call_data().back(), test::kPrinterName,
                          kDummyInitiatorName);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsHiLocale) {
  InitializeWithLocale("hi");

  // Verify initial settings were sent for Hindi.
  ValidateInitialSettingsForLocale(*web_ui()->call_data().back(),
                                   test::kPrinterName, kDummyInitiatorName,
                                   "hi", ",", ".");
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRuLocale) {
  InitializeWithLocale("ru");

  // Verify initial settings were sent for Russian.
  ValidateInitialSettingsForLocale(*web_ui()->call_data().back(),
                                   test::kPrinterName, kDummyInitiatorName,
                                   "ru", "\xC2\xA0", ",");
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsNoPolicies) {
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", std::nullopt,
                                                  std::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground", std::nullopt,
                                                  std::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", std::nullopt, std::nullopt);
  ValidateInitialSettingsValuePolicy(*web_ui()->call_data().back(), "sheets",
                                     std::nullopt);
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrintPreviewHandlerTest, InitialSettingsNoAsh) {
  DisableAshChrome();
  Initialize();
  // Verify initial settings were sent.
  ValidateInitialSettings(*web_ui()->call_data().back(), test::kPrinterName,
                          kDummyInitiatorName);
  // Verify policy settings are empty.
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", std::nullopt,
                                                  std::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground", std::nullopt,
                                                  std::nullopt);
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", std::nullopt, std::nullopt);
  ValidateInitialSettingsValuePolicy(*web_ui()->call_data().back(), "sheets",
                                     std::nullopt);
}
#endif

TEST_F(PrintPreviewHandlerTest, InitialSettingsRestrictHeaderFooterEnabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.print_header_footer_allowed =
      crosapi::mojom::Policies::OptionalBool::kTrue;
  SetPolicies(policies);
#else
  // Set a pref with allowed value.
  prefs()->SetManagedPref(prefs::kPrintHeaderFooter,
                          std::make_unique<base::Value>(true));
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "headerFooter", base::Value(true),
      std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRestrictHeaderFooterDisabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.print_header_footer_allowed =
      crosapi::mojom::Policies::OptionalBool::kFalse;
  SetPolicies(policies);
#else
  // Set a pref with allowed value.
  prefs()->SetManagedPref(prefs::kPrintHeaderFooter,
                          std::make_unique<base::Value>(false));
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "headerFooter", base::Value(false),
      std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableHeaderFooter) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.print_header_footer_default =
      crosapi::mojom::Policies::OptionalBool::kTrue;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetBoolean(prefs::kPrintHeaderFooter, true);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", std::nullopt,
                                                  base::Value(true));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDisableHeaderFooter) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.print_header_footer_default =
      crosapi::mojom::Policies::OptionalBool::kFalse;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetBoolean(prefs::kPrintHeaderFooter, false);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "headerFooter", std::nullopt,
                                                  base::Value(false));
}

TEST_F(PrintPreviewHandlerTest,
       InitialSettingsRestrictBackgroundGraphicsEnabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.allowed_background_graphics_modes =
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled;
  SetPolicies(policies);
#else
  // Set a pref with allowed value.
  prefs()->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 1);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground",
                                                  base::Value(1), std::nullopt);
}

TEST_F(PrintPreviewHandlerTest,
       InitialSettingsRestrictBackgroundGraphicsDisabled) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.allowed_background_graphics_modes =
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled;
  SetPolicies(policies);
#else
  // Set a pref with allowed value.
  prefs()->SetInteger(prefs::kPrintingAllowedBackgroundGraphicsModes, 2);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground",
                                                  base::Value(2), std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableBackgroundGraphics) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.background_graphics_default =
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kEnabled;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 1);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground", std::nullopt,
                                                  base::Value(1));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDisableBackgroundGraphics) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.background_graphics_default =
      crosapi::mojom::Policies::BackgroundGraphicsModeRestriction::kDisabled;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingBackgroundGraphicsDefault, 2);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(*web_ui()->call_data().back(),
                                                  "cssBackground", std::nullopt,
                                                  base::Value(2));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultPaperSizeName) {
  const char kExpectedInitialSettingsPolicy[] = R"(
    {
      "width": 148000,
      "height": 210000
    })";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.paper_size_default = gfx::Size(148000, 210000);
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  const char kPrintingPaperSizeDefaultName[] = R"(
    {
      "name": "iso_a5_148x210mm"
    })";
  prefs()->Set(prefs::kPrintingPaperSizeDefault,
               base::test::ParseJson(kPrintingPaperSizeDefaultName));
#endif
  Initialize();

  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", std::nullopt,
      base::test::ParseJson(kExpectedInitialSettingsPolicy));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultPaperSizeCustomSize) {
  const char kExpectedInitialSettingsPolicy[] = R"(
    {
      "width": 148000,
      "height": 210000
    })";

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.paper_size_default = gfx::Size(148000, 210000);
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  const char kPrintingPaperSizeDefaultCustomSize[] = R"(
    {
      "name": "custom",
      "custom_size": {
        "width": 148000,
        "height": 210000
      }
    })";
  prefs()->Set(prefs::kPrintingPaperSizeDefault,
               base::test::ParseJson(kPrintingPaperSizeDefaultCustomSize));
#endif
  Initialize();

  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "mediaSize", std::nullopt,
      base::test::ParseJson(kExpectedInitialSettingsPolicy));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrintPreviewHandlerTest, InitialSettingsMaxSheetsAllowedPolicy) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.max_sheets_allowed = 2;
  policies.max_sheets_allowed_has_value = true;
  SetPolicies(policies);
#else
  prefs()->SetInteger(prefs::kPrintingMaxSheetsAllowed, 2);
#endif
  Initialize();
  ValidateInitialSettingsValuePolicy(*web_ui()->call_data().back(), "sheets",
                                     base::Value(2));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableColorAndMonochrome) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.allowed_color_modes = 3;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingAllowedColorModes, 3);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "color", base::Value(3), std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultColor) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.default_color_mode = printing::mojom::ColorModeRestriction::kColor;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingColorDefault, 2);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "color", std::nullopt, base::Value(2));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsEnableSimplexAndDuplex) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.allowed_duplex_modes = 7;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingAllowedDuplexModes, 7);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "duplex", base::Value(7), std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultSimplex) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.default_duplex_mode =
      printing::mojom::DuplexModeRestriction::kSimplex;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingDuplexDefault, 1);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "duplex", std::nullopt, base::Value(1));
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsRestrictPin) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.allowed_pin_modes = printing::mojom::PinModeRestriction::kPin;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingAllowedPinModes, 1);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "pin", base::Value(1), std::nullopt);
}

TEST_F(PrintPreviewHandlerTest, InitialSettingsDefaultNoPin) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  crosapi::mojom::Policies policies;
  policies.default_pin_mode = printing::mojom::PinModeRestriction::kNoPin;
  SetPolicies(policies);
#else
  // Set a pref that should take priority over StickySettings.
  prefs()->SetInteger(prefs::kPrintingPinDefault, 2);
#endif
  Initialize();
  ValidateInitialSettingsAllowedDefaultModePolicy(
      *web_ui()->call_data().back(), "pin", std::nullopt, base::Value(2));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(PrintPreviewHandlerTest, GetPrinters) {
  base::HistogramTester histogram_tester;
  Initialize();

  // Check all three printer types that implement
  for (size_t i = 0; i < std::size(kFetchableTypes); i++) {
    mojom::PrinterType type = kFetchableTypes[i];
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
  histogram_tester.ExpectTotalCount("PrintPreview.GetPrintersTime.Extension",
                                    1);
  histogram_tester.ExpectTotalCount("PrintPreview.GetPrintersTime.Local", 1);
  histogram_tester.ExpectTotalCount("PrintPreview.GetPrintersTime.PDF", 0);
}

// Validates the 'printing.printer_type_deny_list' pref by placing the extension
// printer type on a deny list. A 'getPrinters' Web UI message is
// then called both fetchable printer types; only local printers should
// be successfully fetched.
TEST_F(PrintPreviewHandlerTest, GetNoDenyListPrinters) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  AddToDenyList(mojom::PrinterType::kExtension);
#else
  base::Value::List deny_list;
  deny_list.Append("extension");
  prefs()->SetList(prefs::kPrinterTypeDenyList, std::move(deny_list));
#endif
  Initialize();

  size_t expected_callbacks = 1;
  for (size_t i = 0; i < std::size(kFetchableTypes); i++) {
    mojom::PrinterType type = kFetchableTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinters(type, callback_id_in);

    // Start with 1 call from initial settings, then add 2 more for each printer
    // type that isn't on the deny list (one for printers-added, and one for the
    // response), and only 1 more for each type on the deny list (just for
    // response).
    const bool is_allowed_type = type == mojom::PrinterType::kLocal;
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
  for (size_t i = 0; i < std::size(kAllSupportedTypes); i++) {
    mojom::PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, test::kPrinterName);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(1u + (i + 1), web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, /*expect_resolved=*/true);
  }

  // Run through the loop again, this time with a printer that has no
  // capabilities.
  for (size_t i = 0; i < std::size(kAllSupportedTypes); i++) {
    mojom::PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" +
        base::NumberToString(i + std::size(kAllSupportedTypes) + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, kEmptyPrinterName);
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings plus
    // std::size(kAllSupportedTypes) from first loop, then add 1 more for each
    // loop iteration.
    ASSERT_EQ(1u + std::size(kAllSupportedTypes) + (i + 1),
              web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, /*expect_resolved=*/false);
  }
}

// Validates the 'printing.printer_type_deny_list' pref by placing the local and
// PDF printer types on the deny list. A 'getPrinterCapabilities' Web UI message
// is then called for all supported printer types; only extension
// printer capabilities should be successfully fetched.
TEST_F(PrintPreviewHandlerTest, GetNoDenyListPrinterCapabilities) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  AddToDenyList(mojom::PrinterType::kLocal);
  AddToDenyList(mojom::PrinterType::kPdf);
#else
  base::Value::List deny_list;
  deny_list.Append("local");
  deny_list.Append("pdf");
  prefs()->SetList(prefs::kPrinterTypeDenyList, std::move(deny_list));
#endif
  Initialize();

  // Check all four printer types that implement
  // PrinterHandler::StartGetCapability().
  for (size_t i = 0; i < std::size(kAllSupportedTypes); i++) {
    mojom::PrinterType type = kAllSupportedTypes[i];
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    handler()->reset_calls();
    SendGetPrinterCapabilities(type, callback_id_in, test::kPrinterName);

    const bool is_allowed_type = type == mojom::PrinterType::kExtension;
    EXPECT_EQ(is_allowed_type, handler()->CalledOnlyForType(type));

    // Start with 1 call from initial settings, then add 1 more for each loop
    // iteration.
    ASSERT_EQ(1u + (i + 1), web_ui()->call_data().size());

    ValidatePrinterCapabilities(callback_id_in, is_allowed_type);
  }
}

TEST_F(PrintPreviewHandlerTest, GetPrinterCapabilitiesContinuousMedia) {
  // Add two media sizes to our printer - one with discrete sizes and one with
  // continuous feed.  The continuous feed media should get filtered out when
  // the capabilities are retrieved.
  base::Value::Dict media_1;
  media_1.Set("width_microns", 100);
  media_1.Set("height_microns", 200);
  base::Value::Dict media_2;
  media_2.Set("width_microns", 300);
  media_2.Set("is_continuous_feed", true);
  // After filtering, the expected media will just have the discrete media.
  base::Value::List expected_media;
  expected_media.Append(media_1.Clone());

  base::Value::List option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));
  base::Value::Dict media_size;
  media_size.Set("option", std::move(option_list));
  base::Value::Dict printer;
  printer.Set("media_size", std::move(media_size));
  base::Value::Dict cdd;
  cdd.Set("printer", std::move(printer));

  ASSERT_EQ(1u, printers().size());
  printers()[0].capabilities.Set(kSettingCapabilities, std::move(cdd));
  printer_handler()->SetPrinters(printers());

  const base::Value::Dict* capabilities =
      printers()[0].capabilities.FindDict(kSettingCapabilities);
  ASSERT_TRUE(capabilities);
  const base::Value::List* options = GetMediaSizeOptionsFromCdd(*capabilities);

  ASSERT_TRUE(options);
  EXPECT_EQ(2u, options->size());

  // Initial settings first to enable javascript.
  Initialize();

  std::string callback_id_in = "test-callback-id";
  handler()->reset_calls();
  SendGetPrinterCapabilities(mojom::PrinterType::kLocal, callback_id_in,
                             test::kPrinterName);
  ASSERT_EQ(2u, web_ui()->call_data().size());

  // Validate printer capabilities only has one media type (the continuous feed
  // option should get filtered out).
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  CheckWebUIResponse(data, callback_id_in, true);
  ASSERT_TRUE(data.arg3()->is_dict());
  const base::Value::Dict& settings = data.arg3()->GetDict();
  capabilities = settings.FindDict(kSettingCapabilities);
  ASSERT_TRUE(capabilities);
  options = GetMediaSizeOptionsFromCdd(*capabilities);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_media, *options);
}

TEST_F(PrintPreviewHandlerTest, Print) {
  Initialize();

  // All printer types can print.
  for (size_t i = 0; i < std::size(kAllTypes); i++) {
    base::HistogramTester histograms;
    handler()->reset_calls();

    // Send print preview request.
    base::Value::Dict preview_ticket = GetPrintPreviewTicket();
    preview_ticket.Set(kPreviewRequestID, static_cast<int>(i));
    std::string preview_callback_id =
        "test-callback-id-" + base::NumberToString(2 * i + 1);
    base::Value::List preview_list_args =
        ConstructPreviewArgs(preview_callback_id, preview_ticket);
    handler()->HandleGetPreview(preview_list_args);

    // Send printing request.
    mojom::PrinterType type = kAllTypes[i];
    base::Value::List print_args;
    std::string print_callback_id =
        "test-callback-id-" + base::NumberToString(2 * (i + 1));
    print_args.Append(print_callback_id);
    base::Value print_ticket(test::GetPrintTicket(type));
    std::string json;
    base::JSONWriter::Write(print_ticket, &json);
    print_args.Append(json);
    handler()->HandleDoPrint(print_args);

    CheckHistograms(histograms, type);

    // Verify correct PrinterHandler was called.
    EXPECT_TRUE(handler()->CalledOnlyForType(type));

    // Verify print promise was resolved successfully.
    const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
    CheckWebUIResponse(data, print_callback_id, true);
  }
}

TEST_F(PrintPreviewHandlerTest, GetPreview) {
  Initialize();

  base::RunLoop run_loop;
  TestPrintPreviewPrintRenderFrame print_render_frame(
      GetInitiatorAssociatedInterfaceProvider());
  print_render_frame.SetCompletionClosure(run_loop.QuitClosure());

  base::Value::Dict print_ticket = GetPrintPreviewTicket();
  base::Value::List list_args =
      ConstructPreviewArgs("test-callback-id-1", print_ticket);
  handler()->HandleGetPreview(list_args);
  run_loop.Run();

  // Verify that the preview was requested from the renderer with the
  // appropriate settings.
  const base::Value::Dict& preview_params = print_render_frame.GetSettings();
  bool preview_id_found = false;
  for (auto it : preview_params) {
    if (it.first == kPreviewUIID) {  // This is added by the handler.
      preview_id_found = true;
      continue;
    }
    const base::Value* value_in = print_ticket.Find(it.first);
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
  base::Value::Dict print_ticket = GetPrintPreviewTicket();
  base::Value::List list_args =
      ConstructPreviewArgs(callback_id_in, print_ticket);
  handler()->HandleGetPreview(list_args);
  run_loop.Run();
  const base::Value::Dict& preview_params = print_render_frame.GetSettings();

  // Read the preview UI ID and request ID
  std::optional<int> request_value = preview_params.FindInt(kPreviewRequestID);
  ASSERT_TRUE(request_value.has_value());
  int preview_request_id = request_value.value();

  std::optional<int> ui_value = preview_params.FindInt(kPreviewUIID);
  ASSERT_TRUE(ui_value.has_value());
  int preview_ui_id = ui_value.value();

  // Simulate renderer responses: PageLayoutReady, PageCountReady,
  // PagePreviewReady, and OnPrintPreviewReady will be called in that order.
  base::Value::Dict layout;
  layout.Set(kSettingMarginTop, 34.0);
  layout.Set(kSettingMarginLeft, 34.0);
  layout.Set(kSettingMarginBottom, 34.0);
  layout.Set(kSettingMarginRight, 34.0);
  layout.Set(kSettingContentWidth, 544.0);
  layout.Set(kSettingContentHeight, 700.0);
  layout.Set(kSettingPrintableAreaX, 17);
  layout.Set(kSettingPrintableAreaY, 17);
  layout.Set(kSettingPrintableAreaWidth, 578);
  layout.Set(kSettingPrintableAreaHeight, 734);
  handler()->SendPageLayoutReady(std::move(layout),
                                 /*all_pages_have_custom_size,=*/false,
                                 /*all_pages_have_custom_orientation,=*/false,
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
  handler()->SendPageLayoutReady(base::Value::Dict(),
                                 /*all_pages_have_custom_size,=*/false,
                                 /*all_pages_have_custom_orientation,=*/false,
                                 preview_request_id);
  EXPECT_EQ(message_count, web_ui()->call_data().size());
  handler()->SendPageCountReady(1, -1, 0);
  EXPECT_EQ(message_count, web_ui()->call_data().size());
  handler()->OnPrintPreviewReady(0, 0);
  EXPECT_EQ(message_count, web_ui()->call_data().size());

  // Handler should have tried to kill the renderer for each of these.
  EXPECT_EQ(handler()->bad_messages(), 3);
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Tests that the `isDriveMounted` setting is only included for the primary
// profile on Lacros.
TEST_F(PrintPreviewHandlerTest, SaveToDriveLacrosPrimaryProfile) {
  crosapi::mojom::Policies policies;
  SetPolicies(policies);
  TestingProfile* main_profile =
      testing_profile_manager()->CreateTestingProfile("Main Profile",
                                                      /*is_main_profile=*/true);
  SetProfileForInitialSettings(main_profile);
  Initialize();

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::Dict& settings = data.arg3()->GetDict();
  EXPECT_TRUE(settings.FindBool("isDriveMounted").has_value());
}

// Tests that the `isDriveMounted` setting is not included for a non-primary
// profile on Lacros.
TEST_F(PrintPreviewHandlerTest, SaveToDriveLacrosNonPrimaryProfile) {
  crosapi::mojom::Policies policies;
  SetPolicies(policies);
  TestingProfile* secondary_profile =
      testing_profile_manager()->CreateTestingProfile(
          "Secondary Profile",
          /*is_main_profile=*/false);
  SetProfileForInitialSettings(secondary_profile);
  Initialize();

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  const base::Value::Dict& settings = data.arg3()->GetDict();
  EXPECT_FALSE(settings.FindBool("isDriveMounted").has_value());
}

#endif

class FailingTestPrinterHandler : public TestPrinterHandler {
 public:
  explicit FailingTestPrinterHandler(const std::vector<PrinterInfo>& printers)
      : TestPrinterHandler(printers) {}
  FailingTestPrinterHandler(const FailingTestPrinterHandler&) = delete;
  FailingTestPrinterHandler& operator=(const FailingTestPrinterHandler&) =
      delete;
  ~FailingTestPrinterHandler() override = default;

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {
    std::move(callback).Run(base::Value::Dict());
  }
};

class PrintPreviewHandlerFailingTest : public PrintPreviewHandlerTest {
 public:
  PrintPreviewHandlerFailingTest() = default;
  PrintPreviewHandlerFailingTest(const PrintPreviewHandlerFailingTest&) =
      delete;
  PrintPreviewHandlerFailingTest& operator=(
      const PrintPreviewHandlerFailingTest&) = delete;
  ~PrintPreviewHandlerFailingTest() override = default;

  std::unique_ptr<TestPrinterHandler> CreatePrinterHandler(
      const std::vector<PrinterInfo>& printers) override {
    return std::make_unique<FailingTestPrinterHandler>(printers);
  }
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
  for (size_t i = 0; i < std::size(kAllSupportedTypes); i++) {
    mojom::PrinterType type = kAllSupportedTypes[i];
    handler()->reset_calls();
    base::Value::List args;
    std::string callback_id_in =
        "test-callback-id-" + base::NumberToString(i + 1);
    args.Append(callback_id_in);
    args.Append(test::kPrinterName);
    args.Append(static_cast<int>(type));
    handler()->HandleGetPrinterCapabilities(args);
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

#if BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)
class ContentAnalysisPrintPreviewHandlerTest
    : public PrintPreviewHandlerTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    enterprise_connectors::ContentAnalysisDelegate::SetFactoryForTesting(
        base::BindRepeating(
            &enterprise_connectors::test::FakeContentAnalysisDelegate::Create,
            run_loop_.QuitClosure(),
            base::BindRepeating(
                &ContentAnalysisPrintPreviewHandlerTest::ScanningResponse,
                base::Unretained(this)),
            kFakeDmToken));

    enterprise_connectors::ContentAnalysisDelegate::DisableUIForTesting();
    PrintPreviewHandlerTest::SetUp();

    // Set the policy that enables local content analysis for print.
    enterprise_connectors::test::SetAnalysisConnector(
        profile()->GetPrefs(), enterprise_connectors::AnalysisConnector::PRINT,
        R"({
          "service_provider": "local_system_agent",
          "enable": [ {"url_list": ["*"], "tags": ["dlp"]} ],
          "block_until_verdict": 1
        })");
  }

  void TearDown() override {
    PrintPreviewHandlerTest::TearDown();
    enterprise_connectors::ContentAnalysisDelegate::EnableUIAfterTesting();
  }

  std::unique_ptr<TestPrintPreviewHandler> CreateHandler(
      std ::unique_ptr<TestPrinterHandler> printer_handler,
      content::WebContents* web_contents) override {
    return std::make_unique<TestPrintPreviewHandlerForContentAnalysis>(
        std::move(printer_handler), web_contents);
  }

  bool scanning_allows_print() const { return GetParam(); }

  enterprise_connectors::ContentAnalysisResponse ScanningResponse(
      const std::string& content,
      const base::FilePath& path) {
    enterprise_connectors::ContentAnalysisResponse response;

    auto* result = response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);

    if (!scanning_allows_print()) {
      auto* rule = result->add_triggered_rules();
      rule->set_rule_name("blocking_rule_name");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }

    return response;
  }

  void WaitForScan() { run_loop_.Run(); }

 private:
  base::RunLoop run_loop_;

  // This installs a fake SDK manager that creates fake SDK clients when
  // its GetClient() method is called. This is needed so that calls to
  // ContentAnalysisSdkManager::Get()->GetClient() do not fail.
  enterprise_connectors::FakeContentAnalysisSdkManager sdk_manager_;
};

TEST_P(ContentAnalysisPrintPreviewHandlerTest, LocalScanBeforePrinting) {
  TestingProfile* main_profile =
      testing_profile_manager()->CreateTestingProfile("Main Profile",
                                                      /*is_main_profile=*/true);
  SetProfileForInitialSettings(main_profile);
  Initialize();

  base::Value::List print_args;
  print_args.Append(kCallbackId);
  base::Value print_ticket(test::GetPrintTicket(kAllTypes[0]));
  std::string json;
  base::JSONWriter::Write(print_ticket, &json);
  print_args.Append(json);

  handler()->HandleDoPrint(print_args);
  WaitForScan();

  auto* print_preview_handler =
      static_cast<TestPrintPreviewHandlerForContentAnalysis*>(handler());

  // Check if printing was allowed or not depending on the parameter.
  ASSERT_EQ(print_preview_handler->print_called_after_scan(),
            scanning_allows_print());

  // Verify that printing went through or not based on the parameter.
  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  CheckWebUIResponse(data, kCallbackId, scanning_allows_print());
}

INSTANTIATE_TEST_SUITE_P(All,
                         ContentAnalysisPrintPreviewHandlerTest,
                         /*scanning_allows_print=*/testing::Bool());

#endif  // BUILDFLAG(ENTERPRISE_LOCAL_CONTENT_ANALYSIS)

}  // namespace printing
