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
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/printing/print_test_utils.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_utils.h"
#include "chrome/browser/ui/webui/print_preview/printer_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#endif

namespace printing {

namespace {

std::vector<crosapi::mojom::LocalDestinationInfoPtr>
ConvertToLocalDestinationInfo(std::vector<std::string> printer_ids) {
  std::vector<crosapi::mojom::LocalDestinationInfoPtr> local_printers;
  for (const auto& printer_id : printer_ids) {
    crosapi::mojom::LocalDestinationInfoPtr local_printer =
        crosapi::mojom::LocalDestinationInfo::New();
    local_printer->id = printer_id;
    local_printers.push_back(std::move(local_printer));
  }
  return local_printers;
}

}  // namespace

const char kSelectedPrintServerId[] = "selected-print-server-id";
const char kSelectedPrintServerName[] = "Print Server Name";

class TestLocalPrinter : public FakeLocalPrinter {
 public:
  TestLocalPrinter() = default;
  TestLocalPrinter(const TestLocalPrinter&) = delete;
  TestLocalPrinter& operator=(const TestLocalPrinter&) = delete;
  ~TestLocalPrinter() override { EXPECT_FALSE(print_server_ids_); }

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
  void AddLocalPrintersObserver(
      mojo::PendingRemote<crosapi::mojom::LocalPrintersObserver> remote,
      AddLocalPrintersObserverCallback callback) override {
    std::move(callback).Run(std::move(local_printers_));
  }

  void SetLocalPrinters(std::vector<std::string> printer_ids) {
    local_printers_ = ConvertToLocalDestinationInfo(printer_ids);
  }

 private:
  friend class PrintPreviewHandlerChromeOSTest;

  std::vector<crosapi::mojom::LocalDestinationInfoPtr> local_printers_;
  mojo::Remote<crosapi::mojom::PrintServerObserver> remote_;
  std::optional<std::vector<std::string>> print_server_ids_;
  crosapi::mojom::PrintServersConfigPtr config_;
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
  base::Value::Dict basic_info;
  base::Value::Dict capabilities;
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
                  base::Value::Dict settings,
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
  base::Value::List printers_;
  std::map<std::string, base::Value::Dict> printer_capabilities_;
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
  base::Value::Dict cdd;
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
#if BUILDFLAG(IS_CHROMEOS_ASH)
    ASSERT_TRUE(testing_profile_manager_.SetUp());
    crosapi::IdleServiceAsh::DisableForTesting();
    ash::LoginState::Initialize();
    manager_ = crosapi::CreateCrosapiManagerWithTestRegistry();
#endif
    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(&profile_));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

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
    local_printer_ = std::make_unique<TestLocalPrinter>();
    handler_->local_printer_ = local_printer_.get();
    web_ui()->AddMessageHandler(std::move(preview_handler));
    handler_->AllowJavascriptForTesting();

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::make_unique<PrintPreviewHandler>());
    web_ui()->SetController(std::move(preview_ui));
  }

  void TearDown() override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    manager_.reset();
    ash::LoginState::Shutdown();
#endif
  }

  void DisableAshChrome() {
    local_printer_ = nullptr;
    handler_->local_printer_ = nullptr;
  }

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    ASSERT_TRUE(data.arg1()->is_string());
    EXPECT_EQ(event_id, data.arg1()->GetString());
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  void ChangePrintServersConfig(crosapi::mojom::PrintServersConfigPtr config) {
    EXPECT_TRUE(local_printer_->remote_);
    local_printer_->config_ = config.Clone();
    // Call the callback directly instead of through the mojo remote
    // so that it is synchronous.
    handler_->OnPrintServersChanged(std::move(config));
  }
  std::vector<std::string> TakePrintServerIds() {
    return local_printer_->TakePrintServerIds();
  }
  void ChangeServerPrinters() { handler_->OnServerPrintersChanged(); }
  TestPrinterHandlerChromeOS* printer_handler() { return printer_handler_; }
  std::vector<PrinterInfo>& printers() { return printers_; }

  void SetLocalPrinters(std::vector<std::string> printer_ids) {
    local_printer_->SetLocalPrinters(printer_ids);
  }

  void FireOnLocalPrintersUpdated(std::vector<std::string> printer_ids) {
    handler_->OnLocalPrintersUpdated(
        ConvertToLocalDestinationInfo(printer_ids));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int LocalPrinterVersion() {
    return handler_->GetLocalPrinterVersionForTesting();
  }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  std::unique_ptr<crosapi::CrosapiManager> manager_;
#endif
  TestingProfile profile_;
  std::unique_ptr<TestLocalPrinter> local_printer_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  raw_ptr<PrintPreviewHandlerChromeOS> handler_;
  raw_ptr<TestPrinterHandlerChromeOS> printer_handler_;
  std::vector<PrinterInfo> printers_;
};

TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServersNoAsh) {
  DisableAshChrome();

  base::Value::List selected_args;
  base::Value::List selected_ids_js;
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  web_ui()->HandleReceivedMessage("choosePrintServers", selected_args);
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}

TEST_F(PrintPreviewHandlerChromeOSTest, GetPrintServersConfigNoAsh) {
  DisableAshChrome();
  base::Value::List args;
  args.Append("callback_id");
  web_ui()->HandleReceivedMessage("getPrintServersConfig", args);
  EXPECT_EQ("cr.webUIResponse", web_ui()->call_data().back()->function_name());
  EXPECT_EQ(base::Value("callback_id"), *web_ui()->call_data().back()->arg1());
  EXPECT_EQ(base::Value(true), *web_ui()->call_data().back()->arg2());
  EXPECT_EQ(base::Value(), *web_ui()->call_data().back()->arg3());
}

TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServers) {
  base::Value::List selected_args;
  base::Value::List selected_ids_js;
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  base::Value::List none_selected_args;
  base::Value::List none_selected_js;
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
  const base::Value::List* printer_list =
      call_data->arg2()->GetDict().FindList("printServers");
  bool is_single_server_fetching_mode =
      call_data->arg2()
          ->GetDict()
          .FindBool("isSingleServerFetchingMode")
          .value();

  ASSERT_EQ(printer_list->size(), 1u);
  const base::Value::Dict& first_printer = printer_list->front().GetDict();
  EXPECT_EQ(*first_printer.FindString("id"), kSelectedPrintServerId);
  EXPECT_EQ(*first_printer.FindString("name"), kSelectedPrintServerName);
  EXPECT_EQ(is_single_server_fetching_mode, false);

  base::Value::List args;
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

  base::Value::List args;
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
  const base::Value::Dict* cdd_result =
      data.arg3()->GetDict().FindDict(kSettingCapabilities);
  ASSERT_TRUE(cdd_result);
  const base::Value::List* options = GetMediaSizeOptionsFromCdd(*cdd_result);
  ASSERT_TRUE(options);
  EXPECT_EQ(expected_media, *options);
}

// Verify 'getShowManagePrinters' can be called.
TEST_F(PrintPreviewHandlerChromeOSTest, HandleGetCanShowManagePrinters) {
  const std::string callback_id = "callback-id";
  base::Value::List args;
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (int{crosapi::mojom::LocalPrinter::MethodMinVersions::
              kAddLocalPrintersObserverMinVersion} > LocalPrinterVersion()) {
    LOG(ERROR) << "Local printer version incompatible";
    return;
  }
#endif

  const std::vector<std::string> printers{"Printer1", "Printer2", "Printer3"};
  SetLocalPrinters(printers);

  const std::string callback_id = "callback-id";
  base::Value::List args;
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
  FireOnLocalPrintersUpdated(printers);

  const content::TestWebUI::CallData& data = *web_ui()->call_data().back();
  AssertWebUIEventFired(data, "local-printers-updated");
  EXPECT_EQ(printers.size(), data.arg2()->GetList().size());
}

}  // namespace printing
