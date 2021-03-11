// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/cups_printers_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/json/json_string_value_serializer.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/printing_stubs.h"
#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_core_service_impl.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/debug_daemon/debug_daemon_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/select_file_dialog_factory.h"

namespace chromeos {
namespace settings {

class CupsPrintersHandlerTest;

namespace {

// Converts JSON string to base::ListValue object.
// On failure, returns NULL and fills |*error| string.
std::unique_ptr<base::ListValue> GetJSONAsListValue(const std::string& json,
                                                    std::string* error) {
  auto ret = base::ListValue::From(
      JSONStringValueDeserializer(json).Deserialize(nullptr, error));
  if (!ret)
    *error = "Value is not a list.";
  return ret;
}

}  // namespace

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
  base::Optional<Printer> GetPrinter(const std::string& id) const override {
    return Printer();
  }
};

class FakePpdProvider : public PpdProvider {
 public:
  FakePpdProvider() = default;

  void ResolveManufacturers(ResolveManufacturersCallback cb) override {}
  void ResolvePrinters(const std::string& manufacturer,
                       ResolvePrintersCallback cb) override {}
  void ResolvePpdReference(const PrinterSearchData& search_data,
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
                      void* params) override {
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

class CupsPrintersHandlerTest : public testing::Test {
 public:
  CupsPrintersHandlerTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD),
        profile_(),
        web_ui_(),
        printers_handler_() {}
  ~CupsPrintersHandlerTest() override = default;

  void SetUp() override {
    printers_handler_ = CupsPrintersHandler::CreateForTesting(
        &profile_, base::MakeRefCounted<FakePpdProvider>(),
        std::make_unique<StubPrinterConfigurer>(), &printers_manager_);
    printers_handler_->SetWebUIForTest(&web_ui_);
    printers_handler_->RegisterMessages();
  }

 protected:
  // Must outlive |profile_|.
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  content::TestWebUI web_ui_;
  std::unique_ptr<CupsPrintersHandler> printers_handler_;
  TestCupsPrintersManager printers_manager_;
};

TEST_F(CupsPrintersHandlerTest, RemoveCorrectPrinter) {
  DBusThreadManager::Initialize();
  DebugDaemonClient* client = DBusThreadManager::Get()->GetDebugDaemonClient();
  client->CupsAddAutoConfiguredPrinter("testprinter1", "fakeuri",
                                       base::BindOnce(&AddedPrinter));

  const std::string remove_list = R"(
    ["testprinter1", "Test Printer 1"]
  )";
  std::string error;
  std::unique_ptr<base::ListValue> remove_printers(
      GetJSONAsListValue(remove_list, &error));
  ASSERT_TRUE(remove_printers) << "Error deserializing list: " << error;

  web_ui_.HandleReceivedMessage("removeCupsPrinter", remove_printers.get());

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
}

TEST_F(CupsPrintersHandlerTest, VerifyOnlyPpdFilesAllowed) {
  DownloadCoreServiceFactory::GetForBrowserContext(&profile_)
      ->SetDownloadManagerDelegateForTesting(
          std::make_unique<ChromeDownloadManagerDelegate>(&profile_));

  ui::SelectFileDialog::FileTypeInfo expected_file_type_info;
  // We only allow .ppd and .ppd.gz file extensions for our file select dialog.
  expected_file_type_info.extensions.push_back({"ppd"});
  expected_file_type_info.extensions.push_back({"ppd.gz"});
  ui::SelectFileDialog::SetFactory(
      new TestSelectFileDialogFactory(&expected_file_type_info));

  base::Value args(base::Value::Type::LIST);
  args.Append("handleFunctionName");
  web_ui_.HandleReceivedMessage("selectPPDFile",
                                &base::Value::AsListValue(args));
}

}  // namespace settings.
}  // namespace chromeos.
