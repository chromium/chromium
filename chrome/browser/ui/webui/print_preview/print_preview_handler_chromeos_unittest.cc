// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler_chromeos.h"

#include <vector>

#include "ash/constants/ash_features.h"
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

namespace printing {

const char kSelectedPrintServerId[] = "selected-print-server-id";
const char kSelectedPrintServerName[] = "Print Server Name";

class TestPrintServersManager : public chromeos::PrintServersManager {
 public:
  void AddObserver(Observer* observer) override { observer_ = observer; }

  void RemoveObserver(Observer* observer) override { observer_ = nullptr; }

  void ChoosePrintServer(
      const std::vector<std::string>& selected_print_server_ids) override {
    selected_print_server_ids_ = selected_print_server_ids;
  }

  chromeos::PrintServersConfig GetPrintServersConfig() const override {
    return print_servers_config_;
  }

  void ChangePrintServersConfig(const chromeos::PrintServersConfig& config) {
    print_servers_config_ = config;
    observer_->OnPrintServersChanged(config);
  }

  virtual void ChangeServerPrinters(
      const std::vector<chromeos::PrinterDetector::DetectedPrinter>& printers) {
    observer_->OnServerPrintersChanged(printers);
  }

  std::vector<std::string> selected_print_server_ids() {
    return selected_print_server_ids_;
  }

  Observer* observer_;
  std::vector<std::string> selected_print_server_ids_;
  chromeos::PrintServersConfig print_servers_config_;
};

class TestCupsPrintersManager : public chromeos::StubCupsPrintersManager {
 public:
  explicit TestCupsPrintersManager(
      chromeos::PrintServersManager* print_servers_manager)
      : print_servers_manager_(print_servers_manager) {}

  chromeos::PrintServersManager* GetPrintServersManager() const override {
    return print_servers_manager_;
  }

 private:
  chromeos::PrintServersManager* print_servers_manager_;
};

class FakePrintPreviewUI : public PrintPreviewUI {
 public:
  FakePrintPreviewUI(content::WebUI* web_ui,
                     std::unique_ptr<PrintPreviewHandler> handler)
      : PrintPreviewUI(web_ui, std::move(handler)) {}

  ~FakePrintPreviewUI() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakePrintPreviewUI);
};

class PrintPreviewHandlerChromeOSTest : public testing::Test {
 public:
  PrintPreviewHandlerChromeOSTest() = default;
  ~PrintPreviewHandlerChromeOSTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kPrintServerScaling}, {});
    TestingProfile::Builder builder;
    profile_ = builder.Build();

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

    preview_web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile_.get()));
    web_ui_ = std::make_unique<content::TestWebUI>();
    web_ui_->set_web_contents(preview_web_contents_.get());

    auto preview_handler = std::make_unique<PrintPreviewHandlerChromeOS>();
    handler_ = preview_handler.get();
    web_ui()->AddMessageHandler(std::move(preview_handler));
    handler_->AllowJavascriptForTesting();

    auto preview_ui = std::make_unique<FakePrintPreviewUI>(
        web_ui(), std::make_unique<PrintPreviewHandler>());
    web_ui()->SetController(std::move(preview_ui));
  }

  void AssertWebUIEventFired(const content::TestWebUI::CallData& data,
                             const std::string& event_id) {
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    std::string event_fired;
    ASSERT_TRUE(data.arg1()->GetAsString(&event_fired));
    EXPECT_EQ(event_id, event_fired);
  }

  content::TestWebUI* web_ui() { return web_ui_.get(); }
  TestPrintServersManager* print_servers_manager() {
    return print_servers_manager_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<TestPrintServersManager> print_servers_manager_;
  std::unique_ptr<content::WebContents> preview_web_contents_;
  std::unique_ptr<content::TestWebUI> web_ui_;
  PrintPreviewHandlerChromeOS* handler_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandlerChromeOSTest);
};

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
  EXPECT_THAT(print_servers_manager()->selected_print_server_ids(),
              testing::ElementsAre(std::string(kSelectedPrintServerId)));

  web_ui()->HandleReceivedMessage(
      "choosePrintServers", &base::Value::AsListValue(none_selected_args));
  EXPECT_THAT(print_servers_manager()->selected_print_server_ids(),
              testing::IsEmpty());

  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), true);
}

TEST_F(PrintPreviewHandlerChromeOSTest, OnPrintServersChanged) {
  std::vector<chromeos::PrintServer> servers;
  servers.emplace_back(kSelectedPrintServerId, GURL("http://print-server.com"),
                       kSelectedPrintServerName);

  chromeos::PrintServersConfig config;
  config.print_servers = servers;
  config.fetching_mode = chromeos::ServerPrintersFetchingMode::kStandard;
  print_servers_manager()->ChangePrintServersConfig(config);

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
  std::vector<chromeos::PrinterDetector::DetectedPrinter> printers;

  print_servers_manager()->ChangeServerPrinters(printers);

  AssertWebUIEventFired(*web_ui()->call_data().back(),
                        "server-printers-loading");
  EXPECT_EQ(web_ui()->call_data().back()->arg2()->GetBool(), false);
}

}  // namespace printing
