// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/flat_map.h"
#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ui/webui/ash/shortcut_customization/integration_tests/shortcut_customization_test_base.h"

namespace ash {

namespace {

base::flat_map<base::StringPiece, std::vector<base::StringPiece>>
    kCategoryTable({
        {"General", {"General controls", "Apps"}},
        {"Device", {"Media", "Inputs", "Display"}},
        {"Browser",
         {"General", "Browser Navigation", "Pages", "Tabs", "Bookmarks",
          "Developer tools"}},
        {"Text", {"Text navigation", "Text editing"}},
        {"Windows and desks", {"Windows", "Desks"}},
        {"Accessibility",
         {"ChromeVox", "Visibility", "Accessibility navigation"}},
    });

std::string GetSubcategories(const std::string& category) {
  // Safely convert the selector list in `where` to a JSON/JS list.
  base::Value::List selector_list;
  for (const auto& selector : kCategoryTable[category]) {
    selector_list.Append(selector);
  }
  std::string selectors;
  CHECK(base::JSONWriter::Write(selector_list, &selectors));
  return selectors;
}

}  // namespace

class SideNavInteractiveUiTest
    : public ShortcutCustomizationInteractiveUiTestBase {
 public:
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

  auto VerifyActiveNavTabAndSubcategories(const std::string& category,
                                          int category_index) {
    return Steps(
        Log(std::format("Verifying that '{0}' is the active category when "
                        "the Shortcut Customization app is first launched",
                        category)),
        WaitForElementTextContains(webcontents_id_, kActiveNavTabQuery,
                                   category),
        Log(std::format("Verifying subcategories within the '{0}' category",
                        category)),
        CheckJsResult(webcontents_id_,
                      base::StringPrintf(R"(
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
                                         GetSubcategories(category).c_str())));
  }
};

IN_PROC_BROWSER_TEST_F(SideNavInteractiveUiTest, SelectCategoryFromSideNav) {
  RunTestSequence(
      LaunchShortcutCustomizationApp(),
      VerifyActiveNavTabAndSubcategories("General", /*category_index=*/0),
      ExecuteJsAt(webcontents_id_, kDeviceTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Device", /*category_index=*/1),
      ExecuteJsAt(webcontents_id_, kBrowserTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Browser", /*category_index=*/2),
      ExecuteJsAt(webcontents_id_, kTextTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Text", /*category_index=*/3),
      ExecuteJsAt(webcontents_id_, kWindowsDesksTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Windows and desks",
                                         /*category_index=*/4),
      ExecuteJsAt(webcontents_id_, kAccessibilityTabQuery, kClickFn),
      VerifyActiveNavTabAndSubcategories("Accessibility",
                                         /*category_index=*/5));
}

}  // namespace ash
