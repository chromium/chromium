// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/printing_stubs.h"
#include "chrome/browser/ash/printing/test_printer_configurer.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/dbus/debug_daemon/fake_debug_daemon_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

namespace ash::settings {

using ::chromeos::Printer;

class CupsPrintersHandlerTest;

// Callback used for testing CupsAddAutoConfiguredPrinter().
void AddedPrinter(int32_t status) {
  ASSERT_EQ(status, 0);
}

// Callback used for testing CupsRemovePrinter().
void RemovedPrinter(base::OnceClosure quit_closure,
                    bool* expected,
                    bool result) {
  *expected = result;
  std::move(quit_closure).Run();
}

class TestCupsPrintersManager : public StubCupsPrintersManager {
 public:
  absl::optional<Printer> GetPrinter(const std::string& id) const override {
    return printer_;
  }
  bool IsPrinterInstalled(const chromeos::Printer& printer) const override {
    return printer_installed_;
  }

  // Used to configured our test manager for specific tests.
  void SetPrinter(absl::optional<Printer> printer) { printer_ = printer; }
  void SetPrinterInstalled(bool installed) { printer_installed_ = installed; }

 private:
  absl::optional<Printer> printer_ = Printer();
  bool printer_installed_ = true;
};

class FakePpdProvider : public chromeos::PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdReference(const chromeos::PrinterSearchData& search_data,
                           ResolvePpdReferenceCallback cb) override {}
  void ResolvePpd(const Printer::PpdReference& reference,
                  ResolvePpdCallback cb) override {}
  void ResolvePpdLicense(base::StringPiece effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
};

class TestSelectFilePolicy : public ui::SelectFilePolicy {
 public:
  TestSelectFilePolicy& operator=(const TestSelectFilePolicy&) = delete;

  bool CanOpenSelectFileDialog() override { return true; }
  void SelectFileDenied() override {}
};

// A fake ui::SelectFileDialog, which will cancel the file selection instead of
// selecting a file and verify that the extensions are correctly set.
class FakeSelectFileDialog : public ui::SelectFileDialog {
 public:
  FakeSelectFileDialog(Listener* listener,
                       std::unique_ptr<ui::SelectFilePolicy> policy,
                       FileTypeInfo* file_type)
      : ui::SelectFileDialog(listener, std::move(policy)),
        expected_file_type_info_(file_type) {}

  FakeSelectFileDialog(const FakeSelectFileDialog&) = delete;
  FakeSelectFileDialog& operator=(const FakeSelectFileDialog&) = delete;

 protected:
  void SelectFileImpl(Type type,
                      const std::u16string& title,
                      const base::FilePath& default_path,
                      const FileTypeInfo* file_types,
                      int file_type_index,
                      const base::FilePath::StringType& default_extension,
                      gfx::NativeWindow owning_window,
                      void* params,
                      const GURL* caller) override {
    // Check that the extensions we expect match the actual extensions passed
    // from the CupsPrintersHandler.
    VerifyExtensions(file_types);
    // Close the file select dialog.
    listener_->FileSelectionCanceled(params);
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override {}
  bool HasMultipleFileTypeChoicesImpl() override { return false; }

  void VerifyExtensions(const FileTypeInfo* file_types) {
    const std::vector<std::vector<base::FilePath::StringType>>& actual_exts =
        file_types->extensions;
    std::vector<std::vector<base::FilePath::StringType>> expected_exts =
        expected_file_type_info_->extensions;

    for (std::vector<base::FilePath::StringType> actual : actual_exts) {
      bool is_equal = false;
      std::sort(actual.begin(), actual.end());
      for (auto expected_it = expected_exts.begin();
           expected_it != expected_exts.end(); ++expected_it) {
        std::vector<base::FilePath::StringType>& expected = *expected_it;
        std::sort(expected.begin(), expected.end());
        if (expected == actual) {
          is_equal = true;
          expected_exts.erase(expected_it);
          break;
        }
      }
      ASSERT_TRUE(is_equal);
    }
  }

 private:
  ~FakeSelectFileDialog() override = default;

  ui::SelectFileDialog::FileTypeInfo* expected_file_type_info_;
};

// A factory associated with the artificial file picker.
class TestSelectFileDialogFactory : public ui::SelectFileDialogFactory {
 public:
  explicit TestSelectFileDialogFactory(
      ui::SelectFileDialog::FileTypeInfo* expected_file_type_info)
      : expected_file_type_info_(expected_file_type_info) {}

  ui::SelectFileDialog* Create(
      ui::SelectFileDialog::Listener* listener,
      std::unique_ptr<ui::SelectFilePolicy> policy) override {
    // TODO(jimmyxgong): Investigate why using |policy| created by
    // CupsPrintersHandler crashes the test.
    return new FakeSelectFileDialog(listener,
                                    std::make_unique<TestSelectFilePolicy>(),
                                    expected_file_type_info_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  ui::SelectFileDialog::FileTypeInfo* expected_file_type_info_;
};

class CupsPrintersHandlerTest
    : public testing::Test,
      public content::TestWebUI::JavascriptCallObserver {
 public:
  CupsPrintersHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(std::make_unique<TestingProfile>()),
        web_ui_(),
        printers_handler_() {}
  ~CupsPrintersHandlerTest() override = default;

  void SetUp() override {
    printers_handler_ = CupsPrintersHandler::CreateForTesting(
        profile_.get(), base::MakeRefCounted<FakePpdProvider>(),
        std::make_unique<TestPrinterConfigurer>(), &printers_manager_);
    printers_handler_->SetWebUIForTest(&web_ui_);
    printers_handler_->RegisterMessages();
    printers_handler_->AllowJavascriptForTesting();
    web_ui_.AddJavascriptCallObserver(this);
    printing::PrintBackend::SetPrintBackendForTesting(print_backend_.get());
    DebugDaemonClient::InitializeFake();
  }

  void TearDown() override {
    DebugDaemonClient::Shutdown();
    printing::PrintBackend::SetPrintBackendForTesting(nullptr);
    web_ui_.RemoveJavascriptCallObserver(this);
  }

  void OnJavascriptFunctionCalled(
      const content::TestWebUI::CallData& call_data) override {
    run_loop_.Quit();
  }

  void CallRetrieveCupsPpd() {
    base::Value::List args;
    args.Append(kRetrievePpdCallbackName);
    args.Append("printer_id");
    args.Append(kPpdPrinterName);

    web_ui_.HandleReceivedMessage("retrieveCupsPrinterPpd", args);
    run_loop_.Run();
  }

 protected:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<CupsPrintersHandler> printers_handler_;
  TestCupsPrintersManager printers_manager_;
  base::RunLoop run_loop_;
  scoped_refptr<printing::TestPrintBackend> print_backend_ =
      base::MakeRefCounted<printing::TestPrintBackend>();

  const std::string kRetrievePpdCallbackName = "retrievedPpdCallbackName";
  const std::string kPpdPrinterName = "printer_name";
  const std::string kDefaultPpdData = "PPD data used for testing";
  const std::vector<uint8_t> kPpdData{kDefaultPpdData.begin(),
                                      kDefaultPpdData.end()};
  const std::string kJsCallbackName = "cr.webUIResponse";
};

TEST_F(CupsPrintersHandlerTest, RemoveCorrectPrinter) {
  ConciergeClient::InitializeFake(
      /*fake_cicerone_client=*/nullptr);

  DebugDaemonClient* client = DebugDaemonClient::Get();
  client->CupsAddAutoConfiguredPrinter("testprinter1", "fakeuri",
                                       base::BindOnce(&AddedPrinter));

  const std::string remove_list = R"(
    ["testprinter1", "Test Printer 1"]
  )";
  std::string error;
  base::Value remove_printers = base::test::ParseJson(remove_list);
  ASSERT_TRUE(remove_printers.is_list());

  web_ui_.HandleReceivedMessage("removeCupsPrinter", remove_printers.GetList());

  // We expect this printer removal to fail since the printer should have
  // already been removed by the previous call to 'removeCupsPrinter'.
  base::RunLoop run_loop;
  bool expected = true;
  client->CupsRemovePrinter(
      "testprinter1",
      base::BindOnce(&RemovedPrinter, run_loop.QuitClosure(), &expected),
      base::DoNothing());
  run_loop.Run();
  EXPECT_FALSE(expected);

  profile_.reset();
  ConciergeClient::Shutdown();
}

TEST_F(CupsPrintersHandlerTest, VerifyOnlyPpdFilesAllowed) {
  DownloadCoreServiceFactory::GetForBrowserContext(profile_.get())
      ->SetDownloadManagerDelegateForTesting(
          std::make_unique<ChromeDownloadManagerDelegate>(profile_.get()));

  ui::SelectFileDialog::FileTypeInfo expected_file_type_info;
  // We only allow .ppd and .ppd.gz file extensions for our file select dialog.
  expected_file_type_info.extensions.push_back({"ppd"});
  expected_file_type_info.extensions.push_back({"ppd.gz"});
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(&expected_file_type_info));

  base::Value::List args;
  args.Append("handleFunctionName");
  web_ui_.HandleReceivedMessage("selectPPDFile", args);
}

TEST_F(CupsPrintersHandlerTest, ViewPPD) {
  // Test the nominal case where everything works and the PPD gets returned.

  static_cast<FakeDebugDaemonClient*>(DebugDaemonClient::Get())
      ->SetPpdDataForTesting(kPpdData);

  absl::optional<Printer> printer = printers_manager_.GetPrinter("");
  ASSERT_TRUE(printer);
  print_backend_->AddValidPrinter(
      printer->id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  CallRetrieveCupsPpd();

  base::Value::Dict expectedResults;
  expectedResults.Set("ppd", kDefaultPpdData);
  expectedResults.Set("printerName", kPpdPrinterName);

  EXPECT_EQ(kJsCallbackName, web_ui_.call_data().back()->function_name());
  EXPECT_EQ(base::Value(kRetrievePpdCallbackName),
            *web_ui_.call_data().back()->arg1());
  EXPECT_EQ(base::Value(true), *web_ui_.call_data().back()->arg2());
  EXPECT_EQ(expectedResults, *web_ui_.call_data().back()->arg3());
}

TEST_F(CupsPrintersHandlerTest, ViewPPDPrinterNotFound) {
  // Test the case where the printer is not known to the printer manager.

  // Set an empty printer to simluate not being able to find the printer.
  printers_manager_.SetPrinter(absl::optional<Printer>());

  CallRetrieveCupsPpd();

  base::Value::Dict expectedResults;
  expectedResults.Set("printerName", kPpdPrinterName);

  EXPECT_EQ(kJsCallbackName, web_ui_.call_data().back()->function_name());
  EXPECT_EQ(base::Value(kRetrievePpdCallbackName),
            *web_ui_.call_data().back()->arg1());
  EXPECT_EQ(base::Value(false), *web_ui_.call_data().back()->arg2());
  EXPECT_EQ(expectedResults, *web_ui_.call_data().back()->arg3());
}

TEST_F(CupsPrintersHandlerTest, ViewPPDPrinterNotSetup) {
  // Test the case where the printer is known but not setup.

  static_cast<FakeDebugDaemonClient*>(DebugDaemonClient::Get())
      ->SetPpdDataForTesting(kPpdData);

  absl::optional<Printer> printer = printers_manager_.GetPrinter("");
  ASSERT_TRUE(printer);
  print_backend_->AddValidPrinter(
      printer->id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  // This will cause our printer to get set up.
  printers_manager_.SetPrinterInstalled(false);

  CallRetrieveCupsPpd();

  base::Value::Dict expectedResults;
  expectedResults.Set("ppd", kDefaultPpdData);
  expectedResults.Set("printerName", kPpdPrinterName);

  EXPECT_EQ(kJsCallbackName, web_ui_.call_data().back()->function_name());
  EXPECT_EQ(base::Value(kRetrievePpdCallbackName),
            *web_ui_.call_data().back()->arg1());
  EXPECT_EQ(base::Value(true), *web_ui_.call_data().back()->arg2());
  EXPECT_EQ(expectedResults, *web_ui_.call_data().back()->arg3());
}

TEST_F(CupsPrintersHandlerTest, ViewPPDEmptyPPD) {
  // Test the case where an empty PPD is returned from debugd.

  static_cast<FakeDebugDaemonClient*>(DebugDaemonClient::Get())
      ->SetPpdDataForTesting({});

  absl::optional<Printer> printer = printers_manager_.GetPrinter("");
  ASSERT_TRUE(printer);
  print_backend_->AddValidPrinter(
      printer->id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  CallRetrieveCupsPpd();

  base::Value::Dict expectedResults;
  expectedResults.Set("printerName", kPpdPrinterName);

  EXPECT_EQ(kJsCallbackName, web_ui_.call_data().back()->function_name());
  EXPECT_EQ(base::Value(kRetrievePpdCallbackName),
            *web_ui_.call_data().back()->arg1());
  EXPECT_EQ(base::Value(false), *web_ui_.call_data().back()->arg2());
  EXPECT_EQ(expectedResults, *web_ui_.call_data().back()->arg3());
}

}  // namespace ash::settings
