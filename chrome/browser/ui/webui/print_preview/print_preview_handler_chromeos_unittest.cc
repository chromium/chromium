// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/login/users/scoped_account_id_annotator.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/fake_cups_printers_manager.h"
#include "chrome/browser/ash/printing/local_printer.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "components/account_id/account_id.h"
#include "components/account_id/account_id_literal.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

constexpr char kEmail[] = "test@example.com";
constexpr auto kAccountId =
    AccountId::Literal::FromUserEmailGaiaId(kEmail,
                                            GaiaId::Literal("123456789"));

std::vector<chromeos::Printer> ConvertToPrinters(
    std::vector<std::string> printer_ids) {
  std::vector<chromeos::Printer> printers;
  for (const auto& printer_id : printer_ids) {
    printers.push_back(chromeos::Printer(printer_id));
  }
  return printers;
}

}  // namespace

const char kSelectedPrintServerId[] = "selected-print-server-id";
const char kSelectedPrintServerName[] = "Print Server Name";

class TestCrosLocalPrinter : public FakeLocalPrinter {
 public:
  TestCrosLocalPrinter() = default;
  TestCrosLocalPrinter(const TestCrosLocalPrinter&) = delete;
  TestCrosLocalPrinter& operator=(const TestCrosLocalPrinter&) = delete;
  ~TestCrosLocalPrinter() override { EXPECT_FALSE(print_server_ids_); }

  std::vector<std::string> TakePrintServerIds() {
    std::vector<std::string> print_server_ids = std::move(*print_server_ids_);
    print_server_ids_.reset();
    return print_server_ids;
  }

  // crosapi::mojom::LocalPrinter:
  void ChoosePrintServers(const std::vector<std::string>& print_server_ids,
                          ChoosePrintServersCallback callback) override {
    EXPECT_FALSE(print_server_ids_);
    print_server_ids_ = print_server_ids;
  }
  void AddPrintServerObserver(
      mojo::PendingRemote<crosapi::mojom::PrintServerObserver> remote,
      AddPrintServerObserverCallback callback) override {
    EXPECT_FALSE(remote_);
    EXPECT_TRUE(remote);
    remote_ =
        mojo::Remote<crosapi::mojom::PrintServerObserver>(std::move(remote));
    std::move(callback).Run();
  }
  void GetPrintServersConfig(GetPrintServersConfigCallback callback) override {
    ASSERT_TRUE(config_);
    std::move(callback).Run(std::move(config_));
    config_ = nullptr;
  }

 private:
  friend class PrintPreviewHandlerChromeOSTest;

  mojo::Remote<crosapi::mojom::PrintServerObserver> remote_;
  std::optional<std::vector<std::string>> print_server_ids_;
  crosapi::mojom::PrintServersConfigPtr config_;
};

class TestLocalPrinter : public ash::LocalPrinter {
 public:
  TestLocalPrinter() = default;
  TestLocalPrinter(TestLocalPrinter&) = delete;
  TestLocalPrinter& operator=(TestLocalPrinter&) = delete;
  ~TestLocalPrinter() override {}

  void SetLocalPrinters(std::vector<std::string> printer_ids) {
    printers_ = ConvertToPrinters(printer_ids);
  }

  void GetPrinters(const AccountId& accountId,
                   ash::LocalPrinter::GetPrintersCallback cb) override {
    std::move(cb).Run(printers_);
  }

  void GetCapability(const AccountId& accountId,
                     const std::string& id,
                     ash::LocalPrinter::GetCapabilityCallback cb) override {
    NOTREACHED() << "Should not be called by this unittest.";
  }

  void GetStatus(const AccountId& accountId,
                 const std::string& id,
                 ash::LocalPrinter::GetStatusCallback cb) override {
    NOTREACHED() << "Should not be called by this unittest.";
  }

 private:
  std::vector<chromeos::Printer> printers_;
};

class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  FakePrintPreviewUI(content::WebUI* web_ui,
                     std::unique_ptr<PrintPreviewHandler> handler)
      : PrintPreviewUI(web_ui, std::move(handler)) {}
  FakePrintPreviewUI(const FakePrintPreviewUI&) = delete;
  FakePrintPreviewUI& operator=(const FakePrintPreviewUI&) = delete;
  ~FakePrintPreviewUI() override = default;

 private:
};

struct PrinterInfo {
  std::string id;
  bool is_default;
  base::DictValue basic_info;
  base::DictValue capabilities;
};

class TestPrinterHandlerChromeOS : public PrinterHandler {
 public:
  explicit TestPrinterHandlerChromeOS(
      const std::vector<PrinterInfo>& printers) {
    SetPrinters(printers);
  }
  TestPrinterHandlerChromeOS(const TestPrinterHandlerChromeOS&) = delete;
  TestPrinterHandlerChromeOS& operator=(const TestPrinterHandlerChromeOS&) =
      delete;
  ~TestPrinterHandlerChromeOS() override = default;

  void Reset() override {}

  void GetDefaultPrinter(DefaultPrinterCallback cb) override {
    std::move(cb).Run(default_printer_);
  }

  void StartGetPrinters(AddedPrintersCallback added_printers_callback,
                        GetPrintersDoneCallback done_callback) override {
    if (!printers_.empty()) {
      added_printers_callback.Run(printers_.Clone());
    }
    std::move(done_callback).Run();
  }

  void StartGetCapability(const std::string& destination_id,
                          GetCapabilityCallback callback) override {
    std::move(callback).Run(printer_capabilities_[destination_id].Clone());
  }

  void StartGrantPrinterAccess(const std::string& printer_id,
                               GetPrinterInfoCallback callback) override {}

  void StartPrint(const std::u16string& job_title,
                  base::DictValue settings,
                  scoped_refptr<base::RefCountedMemory> print_data,
                  PrintCallback callback) override {
    std::move(callback).Run(base::Value());
  }

  void SetPrinters(const std::vector<PrinterInfo>& printers) {
    printers_.clear();
    for (const auto& printer : printers) {
      if (printer.is_default) {
        default_printer_ = printer.id;
      }
      printers_.Append(printer.basic_info.Clone());
      printer_capabilities_[printer.id] = printer.capabilities.Clone();
    }
  }

 private:
  std::string default_printer_;
  base::ListValue printers_;
  std::map<std::string, base::DictValue> printer_capabilities_;
};

class TestPrintPreviewHandlerChromeOS : public PrintPreviewHandlerChromeOS {
 public:
  explicit TestPrintPreviewHandlerChromeOS(
      std::unique_ptr<PrinterHandler> printer_handler)
      : test_printer_handler_(std::move(printer_handler)) {}

  PrinterHandler* GetPrinterHandler(mojom::PrinterType printer_type) override {
    return test_printer_handler_.get();
  }

 private:
  std::unique_ptr<PrinterHandler> test_printer_handler_;
};

PrinterInfo GetSimplePrinterInfo(const std::string& name, bool is_default) {
  PrinterInfo simple_printer;
  simple_printer.id = name;
  simple_printer.is_default = is_default;
  simple_printer.basic_info.Set("printer_name", simple_printer.id);
  simple_printer.basic_info.Set("printer_description", "Printer for test");
  simple_printer.basic_info.Set("printer_status", 1);
  base::DictValue cdd;
  simple_printer.capabilities.Set("printer", simple_printer.basic_info.Clone());
  simple_printer.capabilities.Set("capabilities", cdd.Clone());
  return simple_printer;
}

class PrintPreviewHandlerChromeOSTest : public testing::Test {
 public:
  PrintPreviewHandlerChromeOSTest() = default;
  PrintPreviewHandlerChromeOSTest(const PrintPreviewHandlerChromeOSTest&) =
      delete;
  PrintPreviewHandlerChromeOSTest& operator=(
      const PrintPreviewHandlerChromeOSTest&) = delete;
  ~PrintPreviewHandlerChromeOSTest() override = default;

  void SetUp() override {
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->GetTestingLocalState());
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    ash::LoginState::Initialize();

    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(kAccountId));
    test_user_session_manager_->LogIn(kAccountId);

    profile_ = profile_manager_->CreateTestingProfile(kEmail);
    ash::AnnotatedAccountId::Set(profile_, kAccountId);
    user_manager::UserManager::Get()->OnUserProfileCreated(
        kAccountId, profile_->GetPrefs());

    manager_ = std::make_unique<crosapi::CrosapiManager>();
    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

    ash::CupsPrintersManagerFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_,
        base::BindLambdaForTesting([this](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          auto printers_manager =
              std::make_unique<ash::FakeCupsPrintersManager>();
          printers_manager_ = printers_manager.get();
          return printers_manager;
        }));

    // Create printer handler.
    printers_.push_back(
        GetSimplePrinterInfo(test::kPrinterName, /*is_default=*/true));
    auto printer_handler =
        std::make_unique<TestPrinterHandlerChromeOS>(printers_);
    printer_handler_ = printer_handler.get();

    auto preview_handler = std::make_unique<TestPrintPreviewHandlerChromeOS>(
        std::move(printer_handler));
    preview_handler->SetInitiatorForTesting(preview_web_contents_.get());
    handler_ = preview_handler.get();
    cros_local_printer_ = std::make_unique<TestCrosLocalPrinter>();
    handler_->cros_local_printer_ = cros_local_printer_.get();
    local_printer_ = std::make_unique<TestLocalPrinter>();
    handler_->local_printer_ = local_printer_.get();
    web_ui()->AddMessageHandler(std::move(preview_handler));
    handler_->AllowJavascriptForTesting();

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::make_unique<PrintPreviewHandler>());
    web_ui()->SetController(std::move(preview_ui));
  }

  void TearDown() override {
    printers_manager_ = nullptr;
    printer_handler_ = nullptr;
    handler_->SetInitiatorForTesting(nullptr);
    handler_ = nullptr;
    web_ui_.reset();
    manager_.reset();
    preview_web_contents_.reset();
    ash::LoginState::Shutdown();
    user_manager::UserManager::Get()->OnUserProfileWillBeDestroyed(kAccountId);
    profile_ = nullptr;
    profile_manager_->DeleteAllTestingProfiles();
    profile_manager_.reset();
    test_user_session_manager_.reset();
  }

  void DisableAshChrome() {
    handler_->cros_local_printer_ = nullptr;
    handler_->local_printer_ = nullptr;
    cros_local_printer_.reset();
    local_printer_.reset();
  }

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(event_id, data.arg1()->GetString());
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  void ChangePrintServersConfig(crosapi::mojom::PrintServersConfigPtr config) {
    EXPECT_TRUE(cros_local_printer_->remote_);
    cros_local_printer_->config_ = config.Clone();
    // Call the callback directly instead of through the mojo remote
    // so that it is synchronous.
    handler_->OnPrintServersChanged(std::move(config));
  }
  std::vector<std::string> TakePrintServerIds() {
    return cros_local_printer_->TakePrintServerIds();
  }
  void ChangeServerPrinters() { handler_->OnServerPrintersChanged(); }
  TestPrinterHandlerChromeOS* printer_handler() { return printer_handler_; }
  std::vector<PrinterInfo>& printers() { return printers_; }

  void SetLocalPrinters(std::vector<std::string> printer_ids) {
    local_printer_->SetLocalPrinters(printer_ids);
  }

  void FireOnLocalPrintersUpdated() { handler_->OnLocalPrintersUpdated(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  std::unique_ptr<crosapi::CrosapiManager> manager_;
  std::unique_ptr<TestCrosLocalPrinter> cros_local_printer_;
  std::unique_ptr<TestLocalPrinter> local_printer_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  raw_ptr<ash::FakeCupsPrintersManager> printers_manager_;
  raw_ptr<PrintPreviewHandlerChromeOS> handler_;
  raw_ptr<TestPrinterHandlerChromeOS> printer_handler_;
  raw_ptr<TestingProfile> profile_;
  std::vector<PrinterInfo> printers_;
};

TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServersNoAsh) {
  DisableAshChrome();

  base::ListValue selected_args;
  base::ListValue selected_ids_js;
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  web_ui()->HandleReceivedMessage("choosePrintServers", selected_args);
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}

TEST_F(PrintPreviewHandlerChromeOSTest, GetPrintServersConfigNoAsh) {
  DisableAshChrome();
  base::ListValue args;
  args.Append("callback_id");
  web_ui()->HandleReceivedMessage("getPrintServersConfig", args);
  EXPECT_EQ("cr.webUIResponse", web_ui()->call_data().back()->function_name());
  EXPECT_EQ(base::Value("callback_id"), *web_ui()->call_data().back()->arg1());
  EXPECT_EQ(base::Value(true), *web_ui()->call_data().back()->arg2());
  EXPECT_EQ(base::Value(), *web_ui()->call_data().back()->arg3());
}

TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServers) {
  base::ListValue selected_args;
  base::ListValue selected_ids_js;
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  base::ListValue none_selected_args;
  base::ListValue none_selected_js;
  none_selected_args.Append(std::move(none_selected_js));

  web_ui()->HandleReceivedMessage("choosePrintServers", selected_args);
  EXPECT_THAT(TakePrintServerIds(),
              testing::ElementsAre(std::string(kSelectedPrintServerId)));
  web_ui()->HandleReceivedMessage("choosePrintServers", none_selected_args);
  EXPECT_THAT(TakePrintServerIds(), testing::IsEmpty());
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}

TEST_F(PrintPreviewHandlerChromeOSTest, OnPrintServersChanged) {
  std::vector<crosapi::mojom::PrintServerPtr> servers;
  servers.push_back(crosapi::mojom::PrintServer::New(
      kSelectedPrintServerId, GURL("http://print-server.com"),
      kSelectedPrintServerName));

  crosapi::mojom::PrintServersConfigPtr config =
      crosapi::mojom::PrintServersConfig::New();
  config->print_servers = std::move(servers);
  config->fetching_mode = ash::ServerPrintersFetchingMode::kStandard;
  ChangePrintServersConfig(std::move(config));
  auto* call_data = web_ui()->call_data().back().get();
  AssertWebUIEventFired(*call_data, "print-servers-config-changed");
  const base::ListValue* printer_list =
      call_data->arg2()->GetDict().FindList("printServers");
  bool is_single_server_fetching_mode =
      call_data->arg2()
          ->GetDict()
          .FindBool("isSingleServerFetchingMode")
          .value();

  ASSERT_EQ(printer_list->size(), 1u);
  const base::DictValue& first_printer = printer_list->front().GetDict();
  EXPECT_EQ(*first_printer.FindString("id"), kSelectedPrintServerId);
  EXPECT_EQ(*first_printer.FindString("name"), kSelectedPrintServerName);
  EXPECT_EQ(is_single_server_fetching_mode, false);

  base::ListValue args;
  args.Append("callback_id");
  web_ui()->HandleReceivedMessage("getPrintServersConfig", args);
  const base::Value kExpectedConfig = base::test::ParseJson(R"({
    "isSingleServerFetchingMode": false,
    "printServers": [ {
      "id": "selected-print-server-id",
      "name": "Print Server Name" } ]
  })");
  EXPECT_EQ("cr.webUIResponse", web_ui()->call_data().back()->function_name());
  EXPECT_EQ(base::Value("callback_id"), *web_ui()->call_data().back()->arg1());
  EXPECT_EQ(base::Value(true), *web_ui()->call_data().back()->arg2());
  EXPECT_EQ(kExpectedConfig, *web_ui()->call_data().back()->arg3());
}

TEST_F(PrintPreviewHandlerChromeOSTest, OnServerPrintersUpdated) {
  ChangeServerPrinters();
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), false);
}

TEST_F(PrintPreviewHandlerChromeOSTest, HandlePrinterSetup) {
  base::DictValue media_1;
  media_1.Set("width_microns", 100);
  media_1.Set("height_microns", 200);
  base::DictValue media_2;
  media_2.Set("width_microns", 300);
  media_2.Set("is_continuous_feed", true);
  // After filtering, the expected media will just have the discrete media.
  base::ListValue expected_media;
  expected_media.Append(media_1.Clone());

  base::ListValue option_list;
  option_list.Append(std::move(media_1));
  option_list.Append(std::move(media_2));
  base::DictValue media_size;
  media_size.Set("option", std::move(option_list));
  base::DictValue printer;
  printer.Set("media_size", std::move(media_size));
  base::DictValue cdd;
  cdd.Set("printer", std::move(printer));

  ASSERT_EQ(1u, printers().size());
  printers()[0].capabilities.Set(kSettingCapabilities, std::move(cdd));
  printer_handler()->SetPrinters(printers());

  base::ListValue args;
  args.Append("callback_id");
  args.Append(test::kPrinterName);
  web_ui()->HandleReceivedMessage("setupPrinter", args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ("callback_id", data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_bool());
  EXPECT_TRUE(data.arg2()->GetBool());
  ASSERT_TRUE(data.arg3()->is_dict());
  const base::DictValue* cdd_result =
      data.arg3()->GetDict().FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd_result);
  const base::ListValue* options = GetMediaSizeOptionsFromCdd(*cdd_result);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_media, *options);
}

// Verify 'getShowManagePrinters' can be called.
TEST_F(PrintPreviewHandlerChromeOSTest, HandleGetCanShowManagePrinters) {
  const std::string callback_id = "callback-id";
  base::ListValue args;
  args.Append(callback_id);
  web_ui()->HandleReceivedMessage("getShowManagePrinters", args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(callback_id, data.arg1()->GetString());
  ASSERT_TRUE(data.arg2()->is_bool());
  ASSERT_TRUE(data.arg2()->GetBool());
}

// Verify 'observeLocalPrinters' can be called.
TEST_F(PrintPreviewHandlerChromeOSTest, HandleObserveLocalPrinters) {
  const std::vector<std::string> printers{"Printer1", "Printer2", "Printer3"};
  SetLocalPrinters(printers);

  const std::string callback_id = "callback-id";
  base::ListValue args;
  args.Append(callback_id);
  web_ui()->HandleReceivedMessage("observeLocalPrinters", args);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  EXPECT_EQ("cr.webUIResponse", data.function_name());
  ASSERT_TRUE(data.arg1()->is_string());
  EXPECT_EQ(callback_id, data.arg1()->GetString());
  // True if ResolveJavascriptCallback and false if RejectJavascriptCallback
  // is called by the handler.
  EXPECT_TRUE(data.arg2()->GetBool());
  EXPECT_EQ(printers.size(), data.arg3()->GetList().size());
}

// Verify 'local-printers-updated' is fired when the observer is triggered.
TEST_F(PrintPreviewHandlerChromeOSTest, FireLocalPrintersUpdated) {
  const std::vector<std::string> printers{"Printer1", "Printer2", "Printer3"};
  SetLocalPrinters(printers);
  FireOnLocalPrintersUpdated();

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  AssertWebUIEventFired(data, "local-printers-updated");
  EXPECT_EQ(printers.size(), data.arg2()->GetList().size());
}

}  // namespace printing
