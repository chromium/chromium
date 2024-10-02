// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/printing/cups_printers_handler.h"

#include <memory>
#include <string>

#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/printscanmgr/fake_printscanmgr_client.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "printing/backend/test_print_backend.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"
#include "url/gurl.h"

namespace ash::settings {

namespace {

constexpr char kSavedPrintersCountHistogramName[] =
    "Printing.CUPS.SavedPrintersCount";

constexpr char kHandlerFunctionName[] = "handlerFunctionName";

void AddPrinterToPrintScanManager(const std::string& printer_id,
                                  const std::string& ppd) {
  printscanmgr::CupsAddManuallyConfiguredPrinterRequest request;
  request.set_name(printer_id);
  request.set_ppd_contents(ppd);
  base::RunLoop run_loop;
  PrintscanmgrClient::Get()->CupsAddManuallyConfiguredPrinter(
      std::move(request),
      base::IgnoreArgs<std::optional<
          printscanmgr::CupsAddManuallyConfiguredPrinterResponse>>(
          run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

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
  void ResolvePpdLicense(std::string_view effective_make_and_model,
                         ResolvePpdLicenseCallback cb) override {}
  void ReverseLookup(const std::string& effective_make_and_model,
                     ReverseLookupCallback cb) override {}

 private:
  ~FakePpdProvider() override {}
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
                      const GURL* caller) override {
    // Check that the extensions we expect match the actual extensions passed
    // from the CupsPrintersHandler.
    VerifyExtensions(file_types);
    // Close the file select dialog.
    listener_->FileSelectionCanceled();
  }

  bool IsRunning(gfx::NativeWindow owning_window) const override {
    return true;
  }
  void ListenerDestroyed() override { listener_ = nullptr; }
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

  raw_ptr<ui::SelectFileDialog::FileTypeInfo> expected_file_type_info_;
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
    return new FakeSelectFileDialog(listener, nullptr,
                                    expected_file_type_info_);
  }

  TestSelectFileDialogFactory(const TestSelectFileDialogFactory&) = delete;
  TestSelectFileDialogFactory& operator=(const TestSelectFileDialogFactory&) =
      delete;

 private:
  raw_ptr<ui::SelectFileDialog::FileTypeInfo> expected_file_type_info_;
};

class MockNewWindowDelegate : public testing::NiceMock<TestNewWindowDelegate> {
 public:
  // TestNewWindowDelegate:
  MOCK_METHOD(void,
              OpenUrl,
              (const GURL& url, OpenUrlFrom from, Disposition disposition),
              (override));
};

class CupsPrintersHandlerTest : public testing::Test {
 public:
  constexpr static const std::string kPpdPrinterName = "printer_name";
  CupsPrintersHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(std::make_unique<TestingProfile>()),
        web_ui_(),
        printers_handler_() {}
  ~CupsPrintersHandlerTest() override = default;

  void SetUp() override {
    printers_handler_ = CupsPrintersHandler::CreateForTesting(
        profile_.get(), base::MakeRefCounted<FakePpdProvider>(),
        &printers_manager_);
    printers_handler_->SetWebUIForTest(&web_ui_);
    printers_handler_->RegisterMessages();
    printers_handler_->AllowJavascriptForTesting();
    printing::PrintBackend::SetPrintBackendForTesting(print_backend_.get());
    PrintscanmgrClient::InitializeFake();

    DownloadCoreServiceFactory::GetForBrowserContext(profile_.get())
        ->SetDownloadManagerDelegateForTesting(
            std::make_unique<ChromeDownloadManagerDelegate>(profile_.get()));
    // Use a temporary directory for downloads.
    ASSERT_TRUE(download_dir_.CreateUniqueTempDir());
    DownloadPrefs* prefs =
        DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager());
    prefs->SetDownloadPath(download_dir_.GetPath());
    prefs->SkipSanitizeDownloadTargetPathForTesting();
  }

  void TearDown() override {
    PrintscanmgrClient::Shutdown();
    printing::PrintBackend::SetPrintBackendForTesting(nullptr);
  }

  void CallRetrieveCupsPpd(const std::string& printer_id,
                           const std::string& license_url = "",
                           const std::string& printer_name = kPpdPrinterName) {
    base::Value::List args;
    args.Append(printer_id);
    args.Append(printer_name);
    args.Append(license_url);

    web_ui_.HandleReceivedMessage("retrieveCupsPrinterPpd", args);
    run_loop_.Run();
  }

  void CallGetCupsSavedPrintersList() {
    base::Value::List args;
    args.Append(kHandlerFunctionName);
    web_ui_.HandleReceivedMessage("getCupsSavedPrintersList", args);
  }

  // Get the contents of the file that was downloaded.  Return true on success,
  // false on error.
  bool GetDownloadedPpdContents(
      std::string& contents,
      const std::string& printer_name = kPpdPrinterName) const {
    const base::FilePath downloads_path =
        DownloadPrefs::FromDownloadManager(profile_->GetDownloadManager())
            ->DownloadPath();
    const base::FilePath filepath =
        downloads_path.Append(printer_name).AddExtension("ppd");
    return base::ReadFileToString(filepath, &contents);
  }

 protected:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  FakeCupsPrintersManager printers_manager_;
  std::unique_ptr<CupsPrintersHandler> printers_handler_;
  base::RunLoop run_loop_;
  scoped_refptr<printing::TestPrintBackend> print_backend_ =
      base::MakeRefCounted<printing::TestPrintBackend>();
  base::ScopedTempDir download_dir_;
  base::HistogramTester histogram_tester_;

  const std::string kDefaultPpdData = "PPD data used for testing";
  const std::string kPpdDataStrWithHeader = R"(*PPD-Adobe: "4.3")";
  const std::string kPpdErrorString =
      base::StringPrintf("Unable to retrieve PPD for %s.",
                         kPpdPrinterName.c_str());

  MockNewWindowDelegate& new_window_delegate() { return new_window_delegate_; }

 private:
  MockNewWindowDelegate new_window_delegate_;
};

TEST_F(CupsPrintersHandlerTest, RemoveCorrectPrinter) {
  ConciergeClient::InitializeFake(
      /*fake_cicerone_client=*/nullptr);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);
  printers_manager_.SetUpPrinter(printer, /*is_automatic_installation=*/true,
                                 base::DoNothing());

  const std::string remove_list = R"(
    [")" + printer.id() + R"(", "Test Printer 1"]
  )";
  std::string error;
  base::Value remove_printers = base::test::ParseJson(remove_list);
  ASSERT_TRUE(remove_printers.is_list());

  EXPECT_TRUE(printers_manager_.IsPrinterInstalled(printer));
  web_ui_.HandleReceivedMessage("removeCupsPrinter", remove_printers.GetList());
  EXPECT_FALSE(printers_manager_.IsPrinterInstalled(printer));

  profile_.reset();
  ConciergeClient::Shutdown();
}

TEST_F(CupsPrintersHandlerTest, VerifyOnlyPpdFilesAllowed) {
  ui::SelectFileDialog::FileTypeInfo expected_file_type_info;
  // We only allow .ppd and .ppd.gz file extensions for our file select dialog.
  expected_file_type_info.extensions.push_back({"ppd"});
  expected_file_type_info.extensions.push_back({"ppd.gz"});
  ui::SelectFileDialog::SetFactory(
      std::make_unique<TestSelectFileDialogFactory>(&expected_file_type_info));

  base::Value::List args;
  args.Append("handleFunctionName");
  web_ui_.HandleReceivedMessage("selectPPDFile", args);
}

TEST_F(CupsPrintersHandlerTest, ViewPPD) {
  // Test the nominal case where everything works and the PPD gets downloaded.

  AddPrinterToPrintScanManager("id", kDefaultPpdData);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  CallRetrieveCupsPpd(printer.id());

  // Check for the downloaded PPD file.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_EQ(contents, kDefaultPpdData);
}

TEST_F(CupsPrintersHandlerTest, ViewPPDWithLicense) {
  // Test the nominal case where everything works and the PPD (with a license)
  // gets returned.

  AddPrinterToPrintScanManager("id", kPpdDataStrWithHeader);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  const std::string license_url("chrome://os-credits/xerox-printing-license");
  CallRetrieveCupsPpd(printer.id(), license_url);

  // Check that the downloaded PPD file contains the license URL.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_THAT(contents, testing::HasSubstr(license_url));
  EXPECT_THAT(contents, testing::HasSubstr(kPpdDataStrWithHeader));
}

TEST_F(CupsPrintersHandlerTest, ViewPPDUnsanitizedFilename) {
  // Test the nominal case where the printer has a name that needs sanitized.
  const std::string printer_name("bad/name");
  const std::string sanitized_name("bad_name");

  AddPrinterToPrintScanManager("id", kDefaultPpdData);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(sanitized_name)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  CallRetrieveCupsPpd(printer.id(), /*license_url=*/"", printer_name);

  // Check for the downloaded PPD file.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents, sanitized_name));
  EXPECT_EQ(contents, kDefaultPpdData);
}

TEST_F(CupsPrintersHandlerTest, ViewPPDWithLicenseBadPpd) {
  // Try to view a PPD that contains a license, but the PPD doesn't start with
  // the expected PPD string, so the license can't be inserted, and the PPD
  // can't be downloaded.

  AddPrinterToPrintScanManager("id", kDefaultPpdData);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  const std::string license_url("chrome://os-credits/xerox-printing-license");
  CallRetrieveCupsPpd(printer.id(), license_url);

  // Check that the downloaded PPD file contains the error message.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_THAT(contents, testing::HasSubstr(kPpdErrorString));
}

TEST_F(CupsPrintersHandlerTest, ViewPPDPrinterNotFound) {
  // Test the case where the printer is not known to the printer manager.
  // No printers were added to CupsPrintersManager.

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  CallRetrieveCupsPpd("printer_id");

  // Check that the downloaded PPD file contains the error message.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_THAT(contents, testing::HasSubstr(kPpdErrorString));
}

TEST_F(CupsPrintersHandlerTest, ViewPPDPrinterNotSetup) {
  // Test the case where the printer is known but not setup.

  AddPrinterToPrintScanManager("id", kDefaultPpdData);

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  CallRetrieveCupsPpd(printer.id());

  // Check for the downloaded PPD file.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_EQ(contents, kDefaultPpdData);
}

TEST_F(CupsPrintersHandlerTest, ViewPPDEmptyPPD) {
  // Test the case where an empty PPD is returned from printscanmgr.

  AddPrinterToPrintScanManager("id", "");

  Printer printer("id");
  printers_manager_.SavePrinter(printer);

  print_backend_->AddValidPrinter(
      printer.id(),
      std::make_unique<printing::PrinterSemanticCapsAndDefaults>(), nullptr);

  EXPECT_CALL(new_window_delegate(),
              OpenUrl(testing::Property(&GURL::ExtractFileName,
                                        testing::StartsWith(kPpdPrinterName)),
                      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
                      ash::NewWindowDelegate::Disposition::kSwitchToTab))
      .WillOnce(testing::InvokeWithoutArgs(&run_loop_, &base::RunLoop::Quit));

  CallRetrieveCupsPpd(printer.id());

  // Check that the downloaded PPD file contains the error message.
  std::string contents;
  EXPECT_TRUE(GetDownloadedPpdContents(contents));
  EXPECT_THAT(contents, testing::HasSubstr(kPpdErrorString));
}

TEST_F(CupsPrintersHandlerTest, GetSavedPrinters) {
  Printer printer("id");
  printer.SetUri("http://printer/uri");
  printers_manager_.SavePrinter(printer);
  Printer printer2("id2");
  printer2.SetUri("http://printer/uri2");
  printers_manager_.SavePrinter(printer2);

  CallGetCupsSavedPrintersList();

  // Expect 2 printers are recorded to the histogram from the `GetPrinters()`
  // result.
  histogram_tester_.ExpectBucketCount(kSavedPrintersCountHistogramName,
                                      /*sample=*/2,
                                      /*expected_count=*/1);
}

// Verify the saved printers are sent to the "local-printers-updated" event when
// the LocalPrintersObserver is triggered.
TEST_F(CupsPrintersHandlerTest, LocalPrintersObserver) {
  Printer printer1("id1");
  Printer printer2("id2");
  printers_manager_.SavePrinter(printer1);
  printers_manager_.SavePrinter(printer2);
  printers_manager_.TriggerLocalPrintersObserver();
  const content::TestWebUI::CallData& data = *web_ui_.call_data().back();
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("local-printers-updated", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_list());
  EXPECT_EQ(2u, data.arg2()->GetList().size());
}

}  // namespace ash::settings
