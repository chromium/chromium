// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <vector>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/printing_stubs.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/local_printer.mojom.h"
#endif

namespace printing {

const char kSelectedPrintServerId[] = "selected-print-server-id";
const char kSelectedPrintServerName[] = "Print Server Name";

#if BUILDFLAG(IS_CHROMEOS_ASH)
class TestPrintServersManager : public chromeos::PrintServersManager {
 public:
  TestPrintServersManager() = default;
  TestPrintServersManager(const TestPrintServersManager&) = delete;
  TestPrintServersManager& operator=(const TestPrintServersManager&) = delete;
  ~TestPrintServersManager() override { EXPECT_FALSE(observer_); }

  virtual void ChangePrintServersConfig(
      const chromeos::PrintServersConfig& config) {
    print_servers_config_ = config;
    observer_->OnPrintServersChanged(config);
  }

  virtual void ChangeServerPrinters(
      const std::vector<chromeos::PrinterDetector::DetectedPrinter>& printers) {
    ASSERT_TRUE(observer_);
    observer_->OnServerPrintersChanged(printers);
  }

  virtual std::vector<std::string> TakePrintServerIds() {
    std::vector<std::string> print_server_ids = std::move(*print_server_ids_);
    print_server_ids_.reset();
    return print_server_ids;
  }

  // chromeos::PrintServersManager::
  void AddObserver(Observer* observer) override {
    EXPECT_FALSE(observer_);
    EXPECT_TRUE(observer);
    observer_ = observer;
  }

  void RemoveObserver(Observer* observer) override {
    EXPECT_TRUE(observer_);
    observer_ = nullptr;
  }

  void ChoosePrintServer(
      const std::vector<std::string>& print_server_ids) override {
    EXPECT_FALSE(print_server_ids_);
    print_server_ids_ = print_server_ids;
  }

  chromeos::PrintServersConfig GetPrintServersConfig() const override {
    ADD_FAILURE();
    return print_servers_config_;
  }

 private:
  Observer* observer_ = nullptr;
  absl::optional<std::vector<std::string>> print_server_ids_;
  chromeos::PrintServersConfig print_servers_config_;
};

class TestCupsPrintersManager : public chromeos::StubCupsPrintersManager {
 public:
  explicit TestCupsPrintersManager(
      chromeos::PrintServersManager* print_servers_manager)
      : print_servers_manager_(print_servers_manager) {}
  TestCupsPrintersManager(const TestCupsPrintersManager&) = delete;
  TestCupsPrintersManager& operator=(const TestCupsPrintersManager&) = delete;
  ~TestCupsPrintersManager() override = default;

  chromeos::PrintServersManager* GetPrintServersManager() const override {
    return print_servers_manager_;
  }

 private:
  chromeos::PrintServersManager* print_servers_manager_;
};

#elif BUILDFLAG(IS_CHROMEOS_LACROS)
class FakeLocalPrinter : public crosapi::mojom::LocalPrinter {
 public:
  FakeLocalPrinter() = default;
  FakeLocalPrinter(const FakeLocalPrinter&) = delete;
  FakeLocalPrinter& operator=(const FakeLocalPrinter&) = delete;
  ~FakeLocalPrinter() override { EXPECT_FALSE(print_server_ids_); }

  std::vector<std::string> TakePrintServerIds() {
    std::vector<std::string> print_server_ids = std::move(*print_server_ids_);
    print_server_ids_.reset();
    return print_server_ids;
  }

  // The only functions called are AddObserver and ChoosePrintServers.
  // crosapi::mojom::LocalPrinter:
  void GetPrinters(GetPrintersCallback callback) override { FAIL(); }
  void GetCapability(const std::string& printer_id,
                     GetCapabilityCallback callback) override {
    FAIL();
  }
  void GetEulaUrl(const std::string& printer_id,
                  GetEulaUrlCallback callback) override {
    FAIL();
  }
  void GetStatus(const std::string& printer_id,
                 GetStatusCallback callback) override {
    FAIL();
  }
  void ShowSystemPrintSettings(
      ShowSystemPrintSettingsCallback callback) override {
    FAIL();
  }
  void CreatePrintJob(crosapi::mojom::PrintJobPtr job,
                      CreatePrintJobCallback callback) override {
    FAIL();
  }
  void GetPrintServersConfig(GetPrintServersConfigCallback callback) override {
    // Remove FAIL() if this function is used in the unit tests.
    FAIL();
  }
  void ChoosePrintServers(const std::vector<std::string>& print_server_ids,
                          ChoosePrintServersCallback callback) override {
    EXPECT_FALSE(print_server_ids_);
    print_server_ids_ = print_server_ids;
  }
  void AddObserver(
      mojo::PendingRemote<crosapi::mojom::PrintServerObserver> remote,
      AddObserverCallback callback) override {
    EXPECT_FALSE(remote_);
    EXPECT_TRUE(remote);
    remote_ =
        mojo::Remote<crosapi::mojom::PrintServerObserver>(std::move(remote));
    std::move(callback).Run();
  }
  void GetPolicies(GetPoliciesCallback callback) override { FAIL(); }

 private:
  friend class PrintPreviewHandlerChromeOSTest;

  mojo::Remote<crosapi::mojom::PrintServerObserver> remote_;
  absl::optional<std::vector<std::string>> print_server_ids_;
};
#endif

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
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPrintServerScaling}, {});
#endif
    TestingProfile::Builder builder;
    profile_ = builder.Build();
#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::CupsPrintersManagerFactory::GetInstance()
        ->SetTestingFactoryAndUse(
            profile_.get(),
            base::BindLambdaForTesting([this](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
              print_servers_manager_ =
                  std::make_unique<TestPrintServersManager>();
              return std::make_unique<TestCupsPrintersManager>(
                  print_servers_manager_.get());
            }));
#endif
    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

    auto preview_handler = std::make_unique<PrintPreviewHandlerChromeOS>();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    preview_handler->local_printer_ = &local_printer_;
#endif
    handler_ = preview_handler.get();
    web_ui()->AddMessageHandler(std::move(preview_handler));
    handler_->AllowJavascriptForTesting();

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::make_unique<PrintPreviewHandler>());
    web_ui()->SetController(std::move(preview_ui));
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void DisableAshChrome() { handler_->local_printer_ = nullptr; }
#endif

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    std::string event_fired;
    ASSERT_TRUE(data.arg1()->GetAsString(&event_fired));
    EXPECT_EQ(event_id, event_fired);
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void ChangePrintServersConfig(const chromeos::PrintServersConfig& config) {
    print_servers_manager_->ChangePrintServersConfig(config);
  }
  std::vector<std::string> TakePrintServerIds() {
    return print_servers_manager_->TakePrintServerIds();
  }
  void ChangeServerPrinters(
      const std::vector<chromeos::PrinterDetector::DetectedPrinter>& printers) {
    print_servers_manager_->ChangeServerPrinters(printers);
  }
  void ChangeServerPrinters() {
    print_servers_manager_->ChangeServerPrinters(
        std::vector<chromeos::PrinterDetector::DetectedPrinter>{});
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  void ChangePrintServersConfig(crosapi::mojom::PrintServersConfigPtr config) {
    EXPECT_TRUE(local_printer_.remote_);
    // Call the callback directly instead of through the mojo remote
    // so that it is synchronous.
    handler_->OnPrintServersChanged(std::move(config));
  }
  std::vector<std::string> TakePrintServerIds() {
    return local_printer_.TakePrintServerIds();
  }
  void ChangeServerPrinters() { handler_->OnServerPrintersChanged(); }
#endif

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<TestPrintServersManager> print_servers_manager_;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  FakeLocalPrinter local_printer_;
#endif
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  PrintPreviewHandlerChromeOS* handler_;
};

#if BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServersNoAsh) {
  DisableAshChrome();

  base::Value selected_args(base::Value::Type::LIST);
  base::Value selected_ids_js(base::Value::Type::LIST);
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  web_ui()->HandleReceivedMessage("choosePrintServers",
                                  &base::Value::AsListValue(selected_args));
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}
#endif

TEST_F(PrintPreviewHandlerChromeOSTest, ChoosePrintServers) {
  base::Value selected_args(base::Value::Type::LIST);
  base::Value selected_ids_js(base::Value::Type::LIST);
  selected_ids_js.Append(kSelectedPrintServerId);
  selected_args.Append(std::move(selected_ids_js));

  base::Value none_selected_args(base::Value::Type::LIST);
  base::Value none_selected_js(base::Value::Type::LIST);
  none_selected_args.Append(std::move(none_selected_js));

  web_ui()->HandleReceivedMessage("choosePrintServers",
                                  &base::Value::AsListValue(selected_args));
  EXPECT_THAT(TakePrintServerIds(),
              testing::ElementsAre(std::string(kSelectedPrintServerId)));
  web_ui()->HandleReceivedMessage(
      "choosePrintServers", &base::Value::AsListValue(none_selected_args));
  EXPECT_THAT(TakePrintServerIds(), testing::IsEmpty());
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}

TEST_F(PrintPreviewHandlerChromeOSTest, OnPrintServersChanged) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::vector<chromeos::PrintServer> servers;
  servers.emplace_back(kSelectedPrintServerId, GURL("http://print-server.com"),
                       kSelectedPrintServerName);

  chromeos::PrintServersConfig config;
  config.print_servers = servers;
  config.fetching_mode = chromeos::ServerPrintersFetchingMode::kStandard;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  std::vector<crosapi::mojom::PrintServerPtr> servers;
  servers.push_back(crosapi::mojom::PrintServer::New(
      kSelectedPrintServerId, GURL("http://print-server.com"),
      kSelectedPrintServerName));

  crosapi::mojom::PrintServersConfigPtr config =
      crosapi::mojom::PrintServersConfig::New();
  config->print_servers = std::move(servers);
  config->fetching_mode = chromeos::ServerPrintersFetchingMode::kStandard;
#endif
  ChangePrintServersConfig(std::move(config));
  auto* call_data = web_ui()->call_data().back().get();
  AssertWebUIEventFired(*call_data, "print-servers-config-changed");
  base::Value::ConstListView printer_list =
      call_data->arg2()->FindListKey("printServers")->GetList();
  bool is_single_server_fetching_mode =
      call_data->arg2()->FindBoolKey("isSingleServerFetchingMode").value();

  ASSERT_EQ(printer_list.size(), 1u);
  const base::Value& first_printer = printer_list.front();
  EXPECT_EQ(*first_printer.FindStringKey("id"), kSelectedPrintServerId);
  EXPECT_EQ(*first_printer.FindStringKey("name"), kSelectedPrintServerName);
  EXPECT_EQ(is_single_server_fetching_mode, false);
}

TEST_F(PrintPreviewHandlerChromeOSTest, OnServerPrintersUpdated) {
  ChangeServerPrinters();
  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), false);
}

}  // namespace printing
