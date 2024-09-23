// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_lookup.h"
#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/shell.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/test/base/ash/interactive/interactive_ash_test.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/test/event_generator.h"

namespace ash {

namespace {

inline int kDeviceId1 = 15;
inline constexpr char kClickFn[] = "e => e.click()";
inline constexpr char kFocusFn[] = "e => e.focus()";

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";

std::string ConvertVectorToJsonList(const std::vector<std::string>& expected) {
  // Safely convert the selector list in `where` to a JSON/JS list.
  base::Value::List selector_list;
  for (const auto& selector : expected) {
    selector_list.Append(selector);
  }
  std::string selectors;
  CHECK(base::JSONWriter::Write(selector_list, &selectors));
  return selectors;
}

class ShortcutCustomizationInteractiveUiTest : public InteractiveAshTest {
 public:
  ShortcutCustomizationInteractiveUiTest() {
    DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShortcutAppWebContentsId);
    webcontents_id_ = kShortcutAppWebContentsId;

    feature_list_.InitWithFeatures(
        {::features::kShortcutCustomization,
         ash::features::kInputDeviceSettingsSplit,
         ash::features::kEnableKeyboardBacklightControlInSettings},
        {});
  }
  // InteractiveAshTest:
  void SetUpOnMainThread() override {
    InteractiveAshTest::SetUpOnMainThread();

    // Set up context for element tracking for InteractiveBrowserTest.
    SetupContextWidget();
    // Ensure the Feedback SWA is installed.
    InstallSystemApps();
  }

  const DeepQuery kCalendarAcceleratorRowQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 93 corresponds to the "Open/Close Calendar" shortcut.
      "accelerator-row[action='93']",
  };

  const DeepQuery kAddShortcutButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#addAcceleratorButton",
  };

  const DeepQuery kDoneButtonQuery{
      "shortcut-customization-app",
      "#editDialog",
      "#doneButton",
  };

  const DeepQuery kActiveNavTabQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "#navigationSelector > navigation-selector",
      "#navigationSelectorMenu > cr-button.navigation-item.selected",
  };

  // Enters lower-case text into the focused html input element.
  auto EnterLowerCaseText(const std::string& text) {
    return Do([&]() {
      for (char c : text) {
        ui_controls::SendKeyPress(
            /*window=*/nullptr,
            static_cast<ui::KeyboardCode>(ui::VKEY_A + (c - 'a')),
            /*control=*/false, /*shift=*/false,
            /*alt=*/false, /*command=*/false);
      }
    });
  }

  const DeepQuery kRedoActionAcceleratorRowQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-3",
      // Text editing subsection
      "#contentWrapper > accelerator-subsection:nth-child(2)",
      "#rowList > accelerator-row:nth-child(10)",
  };

  auto FocusSearchBox() {
    CHECK(webcontents_id_);
    const DeepQuery kSearchBoxQuery{
        "shortcut-customization-app",
        "#searchBoxWrapper > search-box",
        "#search",
        "#searchInput",
    };
    return Steps(ExecuteJsAt(webcontents_id_, kSearchBoxQuery,
                             "(el) => { el.focus(); el.select(); }"));
  }

  ui::test::InteractiveTestApi::StepBuilder SendKeyPressEvent(
      ui::KeyboardCode key,
      int modifier) {
    return Do([key, modifier]() {
      ui::test::EventGenerator(Shell::GetPrimaryRootWindow())
          .PressKey(key, modifier);
    });
  }

  auto OpenEditShortcutDialog(const DeepQuery& query) {
    CHECK(webcontents_id_);
    const auto edit_button_query = query + "cr-icon-button.edit-button";
    return Steps(ExecuteJsAt(webcontents_id_, query, kFocusFn),
                 ExecuteJsAt(webcontents_id_, edit_button_query, kClickFn));
  }

  auto AddCustomShortcut(ui::Accelerator new_accel) {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kAddShortcutButtonQuery, kClickFn),
        InAnyContext(SendAccelerator(webcontents_id_, new_accel)),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto EditDefaultShortcut(ui::Accelerator new_accel) {
    const DeepQuery kEditShortcutButtonQuery{
        "shortcut-customization-app",
        "#editDialog",
        "accelerator-edit-view",
        "#editButton",
    };
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kEditShortcutButtonQuery, kClickFn),
        InAnyContext(SendAccelerator(webcontents_id_, new_accel)),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto ResetShortcut(const DeepQuery& query) {
    CHECK(webcontents_id_);
    const DeepQuery kRestoreDefaultsButtonQuery{
        "shortcut-customization-app",
        "#editDialog",
        "#restoreDefault",
    };
    const auto edit_button_query = query + "cr-icon-button.edit-button";
    return Steps(
        ExecuteJsAt(webcontents_id_, query, kFocusFn),
        ExecuteJsAt(webcontents_id_, edit_button_query, kClickFn),
        ExecuteJsAt(webcontents_id_, kRestoreDefaultsButtonQuery, kClickFn),
        ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto ClickAddShortcutButton() {
    CHECK(webcontents_id_);
    return Steps(
        ExecuteJsAt(webcontents_id_, kAddShortcutButtonQuery, kClickFn));
  }

  auto ClickDoneButton() {
    CHECK(webcontents_id_);
    return Steps(ExecuteJsAt(webcontents_id_, kDoneButtonQuery, kClickFn));
  }

  auto LaunchShortcutCustomizationApp() {
    return Steps(
        Log("Opening Shortcut Customization app"),
        InstrumentNextTab(webcontents_id_, AnyBrowser()), Do([&]() {
          CreateBrowserWindow(GURL(kChromeUIShortcutCustomizationAppURL));
        }),
        WaitForShow(webcontents_id_),
        Log("Waiting for Shortcut Customization app to load"),
        WaitForWebContentsReady(webcontents_id_,
                                GURL(kChromeUIShortcutCustomizationAppURL)));
  }

  // Ensure focusing web contents doesn't accidentally block accelerator
  // processing. When adding new accelerators, this method is called to
  // prevent the system from processing Ash accelerators.
  auto EnsureAcceleratorsAreProcessed() {
    CHECK(webcontents_id_);
    return ExecuteJs(webcontents_id_,
                     "() => "
                     "document.querySelector('shortcut-customization-app')."
                     "shortcutProvider.preventProcessingAccelerators(false)");
  }

  auto SendShortcutAccelerator(ui::Accelerator accel) {
    CHECK(webcontents_id_);
    return Steps(SendAccelerator(webcontents_id_, accel));
  }

  auto AddKeyboard(bool is_external) {
    return Steps(
        Log(base::StringPrintf("Adding %s keyboard",
                               is_external ? "external" : "internal")),
        Do([is_external, this]() {
          int id = kDeviceId1++;
          const auto id_string = base::NumberToString(id);
          ui::KeyboardDevice fake_keyboard(
              /*id=*/id, /*type=*/
              is_external ? ui::InputDeviceType::INPUT_DEVICE_USB
                          : ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
              /*name=*/"Keyboard" + id_string);
          fake_keyboard.sys_path = base::FilePath("path" + id_string);

          fake_keyboard_devices_.push_back(fake_keyboard);
          ui::KeyboardCapability::KeyboardInfo keyboard_info;
          keyboard_info.device_type =
              is_external
                  ? ui::KeyboardCapability::DeviceType::
                        kDeviceExternalGenericKeyboard
                  : ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
          keyboard_info.top_row_layout = ui::KeyboardCapability::
              KeyboardTopRowLayout::kKbdTopRowLayoutDefault;

          Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
              fake_keyboard, std::move(keyboard_info));
          // Calling RunUntilIdle() here is necessary before setting the
          // keyboard devices to prevent the callback from evdev thread to
          // overwrite whatever we sethere below. See
          // `InputDeviceFactoryEvdevProxy::OnStartupScanComplete()`.
          base::RunLoop().RunUntilIdle();
          ui::DeviceDataManagerTestApi().SetKeyboardDevices(
              fake_keyboard_devices_);
          ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

          std::map<std::string, std::string> sysfs_properties;
          std::map<std::string, std::string> sysfs_attributes;
          sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
          fake_udev_.AddFakeDevice(
              fake_keyboard.name, fake_keyboard.sys_path.value(),
              /*subsystem=*/"input", /*devnode=*/std::nullopt,
              /*devtype=*/std::nullopt, std::move(sysfs_attributes),
              std::move(sysfs_properties));
        }));
  }

  auto WaitForShortcutToContainNumAcceleartors(const DeepQuery& query,
                                               const int expected) {
    return Steps(
        Log(base::StringPrintf("Expecting shortcut to contain %d accelerators",
                               expected)),
        CheckJsResultAt(webcontents_id_, query,
                        "e => e.querySelectorAll('accelerator-view').length",
                        expected));
  }

  ui::Accelerator GetDefaultAcceleratorForAction(AcceleratorAction action) {
    return Shell::Get()
        ->accelerator_lookup()
        ->GetAcceleratorsForAction(action)
        .front()
        .accelerator;
  }

  auto VerifyShortcutForAccelerator(const DeepQuery& query,
                                    const std::vector<std::string>& expected) {
    const DeepQuery accel_view_query =
        query + base::StringPrintf("#container > td");
    return Steps(
        WaitForElementExists(webcontents_id_, accel_view_query),
        CheckJsResultAt(webcontents_id_, accel_view_query,
                        base::StringPrintf(
                            R"(
        (el) => {
          let accelViewElements = el.querySelectorAll('accelerator-view');
          const expectedKeys = %s;
          let results = Array.from(accelViewElements).map(view => {
            const shortcutInputKeys =
             Array.from(view.shadowRoot.querySelectorAll('shortcut-input-key'));
              return shortcutInputKeys.every((element, i) => {
                const regex = new RegExp(`^(${expectedKeys[i]})$`);
                return regex.test(element.key);
                      });
          });
          return results.some(res => res);
        }
      )",

                            ConvertVectorToJsonList(expected).c_str())));
  }

  auto VerifyActiveNavTabAndSubcategories(
      const std::string& category,
      int category_index,
      const std::vector<std::string>& expected_subcategories) {
    return Steps(
        Log(base::StringPrintf(
            "Verifying that '%s' is the active category when "
            "the Shortcut Customization app is first launched",
            category.c_str())),
        WaitForElementTextContains(webcontents_id_, kActiveNavTabQuery,
                                   category),
        Log(base::StringPrintf(
            "Verifying subcategories within the '%s' category",
            category.c_str())),
        CheckJsResult(
            webcontents_id_,
            base::StringPrintf(
                R"(
        () => {
          const subsections =
           document.querySelector("shortcut-customization-app")
          .shadowRoot.querySelector("#navigationPanel")
          .shadowRoot.querySelector("#category-%i")
          .shadowRoot.querySelectorAll("#container > accelerator-subsection");
          const expectedSubcategories = %s;
          return Array.from(subsections).every((s, i) => {
            return s.$.title.innerText === expectedSubcategories[i];
          });
        }
      )",
                category_index,
                ConvertVectorToJsonList(expected_subcategories).c_str())));
  }

 protected:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
  base::test::ScopedFeatureList feature_list_;
  ui::ElementIdentifier webcontents_id_;
};

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       ShortcutOnKeyboardPluggedIn) {
  const DeepQuery kToggleLauncherAcceleratorQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "#category-0",
      "#contentWrapper > accelerator-subsection:nth-child(1)",
      "#rowList > accelerator-row:nth-child(1)",
      "#container > td",
  };

  RunTestSequence(
      AddKeyboard(/*is_external=*/false), LaunchShortcutCustomizationApp(),
      Log("Verifying that 'Open/close Launcher' shortcut contains 1 "
          "accelerator"),
      WaitForElementExists(webcontents_id_, kToggleLauncherAcceleratorQuery),
      WaitForShortcutToContainNumAcceleartors(kToggleLauncherAcceleratorQuery,
                                              /*expected=*/1),
      AddKeyboard(/*is_external=*/true),
      Log("Verifying that 'Open/close Launcher' shortcut now contains 2 "
          "accelerators since an external keyboard has been added"),
      WaitForShortcutToContainNumAcceleartors(kToggleLauncherAcceleratorQuery,
                                              /*expected=*/2));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddCustomAcceleratorAndReset) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Verify that the custom shortcut does not open the calendar "
              "before it's added as a shortcut"),
          OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
          AddCustomShortcut(new_accel),
          Log("Adding Search + Ctrl + n as a custom open/close calendar "
              "shortcut"),
          EnsureAcceleratorsAreProcessed(), SendShortcutAccelerator(new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("Custom shortcut opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Custom shortcut closes calendar"),
          ResetShortcut(kCalendarAcceleratorRowQuery),
          Log("Open/Close calendar shortcut reset to defaults"),
          EnsureAcceleratorsAreProcessed(),
          SendShortcutAccelerator(default_accel),
          WaitForShow(kCalendarViewElementId),
          SendShortcutAccelerator(default_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Default shortcut still works"),
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Custom shortcut no longer works"))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       SearchForShortcuts) {
  const DeepQuery kNoSearchResultsContainerQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#noSearchResultsContainer",
  };
  const DeepQuery kClearSearchButtonQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#search",
      "#clearSearch",
  };

  const DeepQuery kSearchRowActionQuery{
      "shortcut-customization-app",
      "#searchBoxWrapper > search-box",
      "#frb0",
      "#searchResultRowInner",
  };

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          Log("Focusing search box"), FocusSearchBox(),
          Log("Searching for shortcut 'hxz' which should have no results"),
          EnterLowerCaseText("hxz"),
          Log("Verifying that no shortcuts were found"),
          WaitForElementTextContains(webcontents_id_,
                                     kNoSearchResultsContainerQuery,
                                     "No search results"),
          Log("Clearing search box"),
          ExecuteJsAt(webcontents_id_, kClearSearchButtonQuery, kClickFn),
          Log("Refocusing search box"), FocusSearchBox(),
          Log("Searching for 'Redo last action' shortcut"),
          EnterLowerCaseText("redo"),
          Log("Verifying that 'Redo last action' search result row is visible"),
          WaitForElementExists(webcontents_id_, kSearchRowActionQuery),
          Log("Navigating to 'Redo last action' accelerator"),
          SendKeyPressEvent(ui::VKEY_RETURN, ui::EF_NONE),
          Log("Verifying that 'Text' nav tab is active and 'Redo last action' "
              "accelerator is visible"),
          WaitForElementTextContains(webcontents_id_, kActiveNavTabQuery,
                                     "Text"),
          ExecuteJsAt(webcontents_id_, kRedoActionAcceleratorRowQuery,
                      "el => { return !!el;}"))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       OpenKeyboardSettings) {
  const DeepQuery kKeyboardSettingsLink{
      "shortcut-customization-app",
      "shortcuts-bottom-nav-content",
      "#keyboardSettingsLink",
  };

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSettingsWebContentsId);
  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InstrumentNextTab(kSettingsWebContentsId, AnyBrowser()),
      ClickElement(webcontents_id_, kKeyboardSettingsLink),
      WaitForWebContentsReady(
          kSettingsWebContentsId,
          GURL(chrome::GetOSSettingsUrl(
              chromeos::settings::mojom::kPerDeviceKeyboardSubpagePath))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       EditDefaultAccelerator) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
          EditDefaultShortcut(new_accel),
          Log("Setting Search + Ctrl + n as the default open/close calendar "
              "shortcut"),
          FocusWebContents(webcontents_id_), EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id_, new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("New accelerator opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("New accelerator closes calendar"),
          SendShortcutAccelerator(default_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Default accelerator no longer opens the calendar"))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddCustomAcceleratorToUnlockedAction) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_N,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  const DeepQuery kCustomAcceleratorViewQuery{
      "shortcut-customization-app",
      "navigation-view-panel#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 93 corresponds to the "Open/Close Calendar" shortcut.
      "accelerator-row[action='93']",
      "#container > td > accelerator-view:nth-child(2)",
  };

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Verify that the custom shortcut does not open the calendar "
              "before it's added as a shortcut"),
          OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
          AddCustomShortcut(new_accel), FocusWebContents(webcontents_id_),
          EnsureAcceleratorsAreProcessed(),
          Log("Adding Search + Ctrl + n as a custom open/close calendar "
              "shortcut"),
          EnsurePresent(webcontents_id_, kCustomAcceleratorViewQuery),
          Log("New shortcut is present in the UI"),
          SendShortcutAccelerator(new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("Custom shortcut opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Custom shortcut closes calendar"),
          ResetShortcut(kCalendarAcceleratorRowQuery),
          Log("Open/Close calendar shortcut reset to defaults"),
          EnsureAcceleratorsAreProcessed(),
          SendShortcutAccelerator(default_accel),
          WaitForShow(kCalendarViewElementId),
          SendShortcutAccelerator(default_accel),
          WaitForHide(kCalendarViewElementId),
          Log("Default shortcut still works"),
          SendShortcutAccelerator(new_accel),
          EnsureNotPresent(kCalendarViewElementId),
          Log("Custom shortcut no longer works"))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       SelectCategoryFromSideNav) {
  DeepQuery kNavigationSelectorQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "navigation-selector",
  };

  const DeepQuery kActiveNavTabQuery =
      kNavigationSelectorQuery + "cr-button.navigation-item.selected";
  const DeepQuery kDeviceTabQuery =
      kNavigationSelectorQuery +
      "#navigationSelectorMenu > cr-button:nth-child(2)";
  const DeepQuery kBrowserTabQuery =
      kNavigationSelectorQuery +
      "#navigationSelectorMenu > cr-button:nth-child(3)";
  const DeepQuery kTextTabQuery =
      kNavigationSelectorQuery +
      "#navigationSelectorMenu > cr-button:nth-child(4)";
  const DeepQuery kWindowsDesksTabQuery =
      kNavigationSelectorQuery +
      "#navigationSelectorMenu > cr-button:nth-child(5)";
  const DeepQuery kAccessibilityTabQuery =
      kNavigationSelectorQuery +
      "#navigationSelectorMenu > cr-button:nth-child(6)";
  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      VerifyActiveNavTabAndSubcategories("General", /*category_index=*/0,
                                         {"General controls", "Apps"}),
      ExecuteJsAt(webcontents_id_, kDeviceTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Device", /*category_index=*/1,
                                         {"Media", "Inputs", "Display"}),
      ExecuteJsAt(webcontents_id_, kBrowserTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories(
          "Browser", /*category_index=*/2,
          {"General", "Browser Navigation", "Pages", "Tabs", "Bookmarks",
           "Developer tools"}),
      ExecuteJsAt(webcontents_id_, kTextTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Text", /*category_index=*/3,
                                         {"Text navigation", "Text editing"}),
      ExecuteJsAt(webcontents_id_, kWindowsDesksTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories(
          "Windows and desks", /*category_index=*/4, {"Windows", "Desks"}),
      ExecuteJsAt(webcontents_id_, kAccessibilityTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories(
          "Accessibility", /*category_index=*/5,
          {"ChromeVox", "Visibility", "Accessibility navigation"}));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddAcceleratorWithConflict) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator new_accel(ui::VKEY_S,
                            ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  const DeepQuery kErrorMessageConflictQuery{
      "shortcut-customization-app",
      "#editDialog",
      "accelerator-edit-view",
      "#acceleratorInfoText",
  };

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
          ClickAddShortcutButton(), SendAccelerator(webcontents_id_, new_accel),
          Log("Attempting to Add Search + Ctrl + s as a custom "
              "open/close calendar "
              "shortcut"),
          EnsurePresent(webcontents_id_, kErrorMessageConflictQuery),
          Log("Verifying the conflict error message is shown"),
          SendAccelerator(webcontents_id_, new_accel), ClickDoneButton(),
          Log("Pressed the shortcut again to bypass the warning message"),
          EnsureAcceleratorsAreProcessed(),
          SendAccelerator(webcontents_id_, new_accel),
          WaitForShow(kCalendarViewElementId),
          Log("New accelerator opens calendar"),
          SendShortcutAccelerator(new_accel),
          WaitForHide(kCalendarViewElementId),
          Log("New accelerator closes calendar"),
          SendShortcutAccelerator(default_accel),
          EnsurePresent(kCalendarViewElementId),
          Log("Default accelerator also opens the calendar"))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddAcceleratorDisruptive) {
  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  ui::Accelerator feedback_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kOpenFeedbackPage);

  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kOsFeedbackWebContentsId);

  const DeepQuery kErrorMessageConflictQuery{
      "shortcut-customization-app",
      "#editDialog",
      "accelerator-edit-view",
      "#acceleratorInfoText",
  };

  const DeepQuery kCancelButtonQuery{"shortcut-customization-app",
                                     "#editDialog", "#pendingAccelerator",
                                     "#cancelButton"};

  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      InAnyContext(Steps(
          OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
          ClickAddShortcutButton(),
          Log("Attempting to Add Alt + Shift + I as a custom open/close "
              "calendar shortcut"),
          SendAccelerator(webcontents_id_, feedback_accel),
          Log("Verifying the error message for a locked accelerator is shown"),
          EnsurePresent(webcontents_id_, kErrorMessageConflictQuery),
          Log("Clicking cancel button to reset edit dialog state"),
          ExecuteJsAt(webcontents_id_, kCancelButtonQuery, kClickFn),
          Log("Closing dialog"), ClickDoneButton(),
          InstrumentNextTab(kOsFeedbackWebContentsId, AnyBrowser()),
          SendShortcutAccelerator(feedback_accel),
          Log("Verifying that 'Open feedback tool' accelerator still works"),
          WaitForShow(kOsFeedbackWebContentsId))));
}

IN_PROC_BROWSER_TEST_F(ShortcutCustomizationInteractiveUiTest,
                       AddMultiShortcutsAndReset) {
  const DeepQuery kResetAllShortcutsButtonQuery{
      "shortcut-customization-app",
      "#bottomNavContentPanel > shortcuts-bottom-nav-content",
      "#restoreAllButton",
  };

  const DeepQuery kConfirmButtonQuery{"shortcut-customization-app",
                                      "#confirmButton"};

  const DeepQuery kOpenQuickSettingsRowQuery{
      "shortcut-customization-app",
      "#navigationPanel",
      "#category-0",
      "#container",
      "accelerator-subsection",
      "tbody#rowList",
      // Action 113 corresponds to the "Open Quick Settings" shortcut.
      "accelerator-row[action='113']",
  };
  ui::Accelerator custom_calendar_accel(
      ui::VKEY_N, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN);

  ui::Accelerator custom_open_quick_settings_accel(ui::VKEY_Q,
                                                   ui::EF_COMMAND_DOWN);

  ui::Accelerator default_accel =
      GetDefaultAcceleratorForAction(AcceleratorAction::kToggleCalendar);
  RunTestSequence(
      AddKeyboard(/*is_external=*/false), LaunchShortcutCustomizationApp(),
      Log("Verifying default shortcut for 'Open/Close Calendar' is visible"),
      VerifyShortcutForAccelerator(kCalendarAcceleratorRowQuery,
                                   {"meta|launcher", "c"}),
      OpenEditShortcutDialog(kCalendarAcceleratorRowQuery),
      Log("Adding custom Calendar shortcut"),
      AddCustomShortcut(custom_calendar_accel),
      Log("Verifying custom shortcut for 'Open/Close Calendar' is visible"),
      VerifyShortcutForAccelerator(kCalendarAcceleratorRowQuery,
                                   {"meta|launcher", "ctrl", "n"}),
      Log("Verifying default shortcut for 'Open Quick Settings' shortcut"),
      VerifyShortcutForAccelerator(kOpenQuickSettingsRowQuery,
                                   {"alt", "shift", "s"}),
      OpenEditShortcutDialog(kOpenQuickSettingsRowQuery),
      Log("Adding custom 'Open Quick Settings' shortcut"),
      AddCustomShortcut(custom_open_quick_settings_accel),
      VerifyShortcutForAccelerator(kOpenQuickSettingsRowQuery,
                                   {"meta|launcher", "q"}),
      SendShortcutAccelerator(custom_calendar_accel),
      WaitForShow(kCalendarViewElementId),
      Log("Custom shortcut opens calendar"),
      SendShortcutAccelerator(custom_calendar_accel),
      Log("Custom shortcut closes calendar"),
      WaitForHide(kCalendarViewElementId),
      SendShortcutAccelerator(custom_open_quick_settings_accel),
      WaitForShow(kQuickSettingsViewElementId),
      SendShortcutAccelerator(custom_open_quick_settings_accel),
      WaitForHide(kQuickSettingsViewElementId),
      Steps(ClickDoneButton(),
            ClickElement(webcontents_id_, kResetAllShortcutsButtonQuery),
            WaitForElementExists(webcontents_id_, kConfirmButtonQuery),
            ClickElement(webcontents_id_, kConfirmButtonQuery),
            SendShortcutAccelerator(default_accel),
            WaitForShow(kCalendarViewElementId),
            SendShortcutAccelerator(default_accel),
            WaitForHide(kCalendarViewElementId),
            Log("Default shortcut still works"))

  );
}

}  // namespace

}  // namespace ash
