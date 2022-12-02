// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/test_crosapi_dependency_registry.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/chromeos/printing/fake_local_printer_chromeos.h"
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#endif

namespace printing {

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

 private:
  friend class PrintPreviewHandlerChromeOSTest;

  mojo::Remote<crosapi::mojom::PrintServerObserver> remote_;
  absl::optional<std::vector<std::string>> print_server_ids_;
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

    auto preview_handler = std::make_unique<PrintPreviewHandlerChromeOS>();
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
  const base::Value::List& printer_list =
      call_data->arg2()->FindListKey("printServers")->GetList();
  bool is_single_server_fetching_mode =
      call_data->arg2()->FindBoolKey("isSingleServerFetchingMode").value();

  ASSERT_EQ(printer_list.size(), 1u);
  const base::Value& first_printer = printer_list.front();
  EXPECT_EQ(*first_printer.FindStringKey("id"), kSelectedPrintServerId);
  EXPECT_EQ(*first_printer.FindStringKey("name"), kSelectedPrintServerName);
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

}  // namespace printing
