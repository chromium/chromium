// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <string_view>

#include "ash/constants/ash_switches.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "base/command_line.h"
#include "base/json/string_escape.h"
#include "base/strings/pattern.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/test/base/chromeos/crosier/ash_integration_test.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_login_mixin.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chromeos/ash/components/dbus/printscanmgr/printscanmgr_client.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/printing_features.h"

namespace ash {

namespace {

constexpr char kDefaultPpd[] = R"(
  *PPD-Adobe: "4.3"
  *FormatVersion: "4.3"
  *FileVersion: "1.0"
  *LanguageVersion: English
  *LanguageEncoding: ISOLatin1
  *PCFileName: "SAMPLE.PPD"
  *Product: "Sample"
  *PSVersion: "(1) 1"
  *ModelName: "Sample"
  *ShortNickName: "Sample"
  *NickName: "Sample"
  *Manufacturer: "Sample"
  *OpenUI *PageSize: PickOne
  *DefaultPageSize: A4
  *PageSize A4/A4: "<</PageSize[595.20 841.68]>>setpagedevice"
  *CloseUI: *PageSize
  *OpenUI *PageRegion: PickOne
  *DefaultPageRegion: A4
  *PageRegion A4/A4: "<</PageRegion[595.20 841.68]>>setpagedevice"
  *CloseUI: *PageRegion
  *DefaultImageableArea: A4
  *ImageableArea A4/A4: "8.40 8.40 586.80 833.28"
  *DefaultPaperDimension: A4
  *PaperDimension A4/A4: "595.20 841.68"
)";

constexpr int kPpdServerPortNumber = 7002;

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsWebContentsId);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kChromeBrowserWebContentsId);

class PrinterSettingsIntegrationTest : public AshIntegrationTest {
 public:
  const DeepQuery kPrinterSettingsPage{
      "os-settings-ui", "os-settings-main", "main-page-container",
      "settings-device-page", "settings-cups-printers"};

  const DeepQuery kAddManualPrinterButton =
      kPrinterSettingsPage + "#addManualPrinterButton";

  const DeepQuery kAddPrinterManuallyDialog = kPrinterSettingsPage +
                                              "#addPrinterDialog" +
                                              "add-printer-manually-dialog";

  const DeepQuery kSavePrinterButton =
      kAddPrinterManuallyDialog + "#addPrinterButton";

  const DeepQuery kNameInputQuery =
      kAddPrinterManuallyDialog + "#printerNameInput";

  const DeepQuery kAddressInputQuery =
      kAddPrinterManuallyDialog + "#printerAddressInput";

  const DeepQuery kProtocolDropdownQuery = kAddPrinterManuallyDialog + "select";

  const DeepQuery kManufacturerModelDialogQuery =
      kPrinterSettingsPage + "#addPrinterDialog" +
      "add-printer-manufacturer-model-dialog";

  const DeepQuery kManufacturerDropdownQuery = kManufacturerModelDialogQuery +
                                               "#manufacturerDropdown" +
                                               "#search" + "#input";

  const DeepQuery kBrotherManufacturerButtonQuery =
      kManufacturerModelDialogQuery + "#manufacturerDropdown" +
      "#dropdown > div > button";

  const DeepQuery kModelDropdownQuery =
      kManufacturerModelDialogQuery + "#modelDropdown" + "#search" + "#input";

  const DeepQuery kBrotherPrinterButtonQuery = kManufacturerModelDialogQuery +
                                               "#modelDropdown" +
                                               "#dropdown > div > button";

  const DeepQuery kAddPrinterButtonQuery =
      kManufacturerModelDialogQuery + "#addPrinterButton";

  const DeepQuery kMoreActionsButtonQuery =
      kPrinterSettingsPage + "#savedPrinters" + "#frb0" + "#moreActions";

  const DeepQuery kEditButtonQuery =
      kPrinterSettingsPage + "#savedPrinters" + "#editButton";

  const DeepQuery kRemoveButtonQuery =
      kPrinterSettingsPage + "#savedPrinters" + "#removeButton";

  const DeepQuery kViewPpdButtonQuery = kPrinterSettingsPage +
                                        "#editPrinterDialog" +
                                        "#ppdLabel > div > cr-button";

  const DeepQuery kEditNameInputQuery =
      kPrinterSettingsPage + "#editPrinterDialog" + "#printerName";

  const DeepQuery kEditSaveButtonQuery =
      kPrinterSettingsPage + "#editPrinterDialog" +
      "add-printer-dialog > div:nth-child(3) > div:nth-child(2) > "
      "cr-button.action-button";

  const DeepQuery kNearbyPrinterButton =
      kPrinterSettingsPage + "#nearbyPrinterToggleButton";

  const DeepQuery kSecondPrinterMoreActionsButtonQuery =
      kPrinterSettingsPage + "#savedPrinters" + "#frb1" + "#moreActions";

  const DeepQuery kNoSavedPrintersQuery =
      kPrinterSettingsPage + "#noSavedPrinters";

  PrinterSettingsIntegrationTest() {
    feature_list_.InitAndEnableFeature(
        printing::features::kAddPrinterViaPrintscanmgr);
    // Keep test running after dismissing login screen.
    set_exit_when_last_browser_closes(false);
    login_mixin().SetMode(ChromeOSIntegrationLoginMixin::Mode::kTestLogin);
  }

  // AshIntegrationTest:
  void SetUp() override {
    ASSERT_TRUE(
        embedded_test_server()->InitializeAndListen(kPpdServerPortNumber));
    embedded_test_server()->RegisterRequestHandler(
        base::BindRepeating(&PrinterSettingsIntegrationTest::HandleRequest,
                            base::Unretained(this)));
    embedded_test_server()->StartAcceptingConnections();

    AshIntegrationTest::SetUp();
  }

  // AshIntegrationTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    AshIntegrationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchNative(ash::switches::kPrintingPpdChannel,
                                     "localhost");
  }

  std::unique_ptr<net::test_server::BasicHttpResponse> CreateHttpResponse(
      std::string_view content) {
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");
    http_response->set_content(content);
    return http_response;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    const std::string request_path = request.GetURL().path();
    std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
        new net::test_server::BasicHttpResponse);
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("application/json");

    if (request_path == "/metadata_v3/locales.json") {
      return CreateHttpResponse("{\"locales\":[\"en\"]}");
    } else if (request_path == "/metadata_v3/manufacturers-en.json") {
      return CreateHttpResponse(
          "{\"filesMap\": {\"Brother\": \"Brother-en.json\"}}");
    } else if (request_path == "/metadata_v3/Brother-en.json") {
      return CreateHttpResponse(
          "{\"printers\": [{\"name\": \"Brother Printer\",\"emm\": "
          "\"brother-printer\"}]}");
    } else if (base::MatchPattern(request_path, "/metadata_v3/index*.json")) {
      return CreateHttpResponse(
          "{\"ppdIndex\": {\"brother-printer\": {\"ppdMetadata\": [ {\"name\": "
          "\"default-ppd.ppd\"}]}}}");
    } else if (request_path == "/ppds_for_metadata_v3/default-ppd.ppd") {
      return CreateHttpResponse(kDefaultPpd);
    } else if (base::MatchPattern(request_path,
                                  "/metadata_v3/reverse_index-en*.json")) {
      return CreateHttpResponse(
          "{\"locale\": \"en\", \"reverseIndex\": {\"brother-printer\": "
          "{\"manufacturer\": \"Brother\", \"model\": \"Brother Printer\"}}}");
    }

    return nullptr;
  }

  auto WaitForCrInputTextContains(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query,
      std::string_view expected) {
    DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kInputTextFound);

    WebContentsInteractionTestUtil::StateChange state_change;
    state_change.type = WebContentsInteractionTestUtil::StateChange::Type::
        kExistsAndConditionTrue;
    state_change.where = query;
    state_change.test_function =
        base::StrCat({"function(el) { return el.value.indexOf(",
                      base::GetQuotedJSONString(expected), ") >= 0; }"});
    state_change.event = kInputTextFound;
    return WaitForStateChange(element_id, state_change);
  }

  auto LaunchOsPrinterSettings() {
    return Steps(
        InstrumentNextTab(kSettingsWebContentsId, AnyBrowser()), Do([&]() {
          chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
              GetActiveUserProfile(),
              chromeos::settings::mojom::kPrintingDetailsSubpagePath);
        }),
        WaitForShow(kSettingsWebContentsId),
        WaitForWebContentsReady(
            kSettingsWebContentsId,
            chrome::GetOSSettingsUrl(
                chromeos::settings::mojom::kPrintingDetailsSubpagePath)));
  }

  auto ReloadOsPrinterSettings() {
    return Steps(
        Do([]() {
          ASSERT_FALSE(BrowserList::GetInstance()->empty());
          chrome::Reload(BrowserList::GetInstance()->GetLastActive(),
                         WindowOpenDisposition::CURRENT_TAB);
        }),
        WaitForHide(kSettingsWebContentsId),
        WaitForShow(kSettingsWebContentsId),
        WaitForWebContentsReady(
            kSettingsWebContentsId,
            chrome::GetOSSettingsUrl(
                chromeos::settings::mojom::kPrintingDetailsSubpagePath)));
  }

  auto WaitForElementToExistAndRender(
      const ui::ElementIdentifier& element_id,
      const WebContentsInteractionTestUtil::DeepQuery& query) {
    return Steps(WaitForElementExists(element_id, query),
                 WaitForElementToRender(element_id, query));
  }

  auto AddPrinterManually(std::string_view printer_name) {
    return Steps(
        Log("Opening the add printer dialog"),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kAddManualPrinterButton),
        ClickElement(kSettingsWebContentsId, kAddManualPrinterButton),
        Log("Inserting the printer name"),
        WaitForElementToExistAndRender(kSettingsWebContentsId, kNameInputQuery),
        ExecuteJsAt(
            kSettingsWebContentsId, kNameInputQuery,
            base::StrCat({"(el) => { el.value = '", printer_name, "'; }"})),
        WaitForCrInputTextContains(kSettingsWebContentsId, kNameInputQuery,
                                   printer_name),
        Log("Inserting the address"),
        ExecuteJsAt(kSettingsWebContentsId, kAddressInputQuery,
                    "(el) => { el.value = 'address'; }"),
        WaitForCrInputTextContains(kSettingsWebContentsId, kAddressInputQuery,
                                   "address"),
        Log("Updating the protocol"),
        ExecuteJsAt(kSettingsWebContentsId, kProtocolDropdownQuery,
                    "(el) => {el.selectedIndex = 4; el.dispatchEvent(new "
                    "Event('change'));}"),
        Log("Click the save button"),
        ClickElement(kSettingsWebContentsId, kSavePrinterButton),
        Log("Adding the manufacturer"),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kManufacturerDropdownQuery),
        ClickElement(kSettingsWebContentsId, kManufacturerDropdownQuery),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kBrotherManufacturerButtonQuery),
        ClickElement(kSettingsWebContentsId, kBrotherManufacturerButtonQuery),
        Log("Adding the model"),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kModelDropdownQuery),
        ClickElement(kSettingsWebContentsId, kModelDropdownQuery),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kBrotherPrinterButtonQuery),
        ClickElement(kSettingsWebContentsId, kBrotherPrinterButtonQuery),
        Log("Saving the printer"),
        ClickElement(kSettingsWebContentsId, kAddPrinterButtonQuery));
  }

  auto EditPrinterName(std::string_view printer_name) {
    return Steps(
        Log("Opening the edit printer dialog"),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kMoreActionsButtonQuery),
        ClickElement(kSettingsWebContentsId, kMoreActionsButtonQuery),
        WaitForElementToExistAndRender(kSettingsWebContentsId,
                                       kEditButtonQuery),
        ClickElement(kSettingsWebContentsId, kEditButtonQuery),
        Log("Editing the printer name"),
        ExecuteJsAt(
            kSettingsWebContentsId, kEditNameInputQuery,
            base::StrCat({"(el) => { el.value = '", printer_name,
                          "'; el.dispatchEvent(new Event('input')) }"})),
        WaitForCrInputTextContains(kSettingsWebContentsId, kEditNameInputQuery,
                                   printer_name),
        Log("Saving the edited printer"),
        ClickElement(kSettingsWebContentsId, kEditSaveButtonQuery));
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrinterSettingsIntegrationTest, ViewPpd) {
  SetupContextWidget();

  login_mixin().Login();

  // Waits for the primary user session to start.
  ash::test::WaitForPrimaryUserSessionStart();

  InstallSystemApps();

  // Set fake data for clicking "View PPD" to return.
  PrintscanmgrClient::InitializeFakeForTest();

  RunTestSequence(
      Log("Launching printer settings"), LaunchOsPrinterSettings(),
      AddPrinterManually("First Printer"),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kMoreActionsButtonQuery),
      Log("Opening the edit dialog"),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kMoreActionsButtonQuery),
      ClickElement(kSettingsWebContentsId, kMoreActionsButtonQuery),
      WaitForElementToExistAndRender(kSettingsWebContentsId, kEditButtonQuery),
      ClickElement(kSettingsWebContentsId, kEditButtonQuery),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kViewPpdButtonQuery),
      Log("Clicking the View PPD button"),
      ScrollIntoView(kSettingsWebContentsId, kViewPpdButtonQuery),
      InstrumentNextTab(kChromeBrowserWebContentsId, AnyBrowser()),
      ClickElement(kSettingsWebContentsId, kViewPpdButtonQuery),
      Log("Verifying the PPD contents"),
      WaitForShow(kChromeBrowserWebContentsId),
      CheckJsResult(kChromeBrowserWebContentsId,
                    "() => document.body.textContent", kDefaultPpd));
}

IN_PROC_BROWSER_TEST_F(PrinterSettingsIntegrationTest, AddAndEditPrinter) {
  SetupContextWidget();

  login_mixin().Login();

  // Waits for the primary user session to start.
  ash::test::WaitForPrimaryUserSessionStart();

  InstallSystemApps();

  // Set fake data for clicking "View PPD" to return.
  PrintscanmgrClient::InitializeFakeForTest();

  RunTestSequence(
      Log("Launching printer settings"), LaunchOsPrinterSettings(),
      Log("Adding the first printer"), AddPrinterManually("First Printer"),
      EditPrinterName("New printer name"), ReloadOsPrinterSettings(),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kNearbyPrinterButton),
      ClickElement(kSettingsWebContentsId, kNearbyPrinterButton),
      Log("Adding the second printer"), AddPrinterManually("Second Printer"),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kSecondPrinterMoreActionsButtonQuery),
      Log("Removing the second printer"),
      ClickElement(kSettingsWebContentsId,
                   kSecondPrinterMoreActionsButtonQuery),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kRemoveButtonQuery),
      ClickElement(kSettingsWebContentsId, kRemoveButtonQuery),
      Log("Removing the first printer"),
      ClickElement(kSettingsWebContentsId, kMoreActionsButtonQuery),
      WaitForElementToExistAndRender(kSettingsWebContentsId,
                                     kRemoveButtonQuery),
      ClickElement(kSettingsWebContentsId, kRemoveButtonQuery),
      Log("Verify the 'No saved printers' section"),
      WaitForElementTextContains(kSettingsWebContentsId, kNoSavedPrintersQuery,
                                 "No saved printers"));
}

}  // namespace
}  // namespace ash
