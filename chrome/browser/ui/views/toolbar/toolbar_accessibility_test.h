// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCESSIBILITY_TEST_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCESSIBILITY_TEST_H_

#include <memory>
#include <string>

#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/scoped_accessibility_mode.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/interaction/element_identifier.h"

namespace ui {
class TrackedElement;
}

class ToolbarAccessibilityTest : public InteractiveBrowserTest,
                                 public testing::WithParamInterface<bool> {
 public:
  ToolbarAccessibilityTest();
  ~ToolbarAccessibilityTest() override;

  void SetUpOnMainThread() override;

 protected:
  // Waits for the initial WebUI to be ready to show if it's enabled.
  void WaitForInitialWebUI(Browser* browser = nullptr);

  // Sets up accessibility for a test and waits for the WebUI renderer to
  // populate accessibility data if |use_webui| is true.
  void ConfigureAccessibilityForWebUITest(bool use_webui);

  // Finds an AXNode with the given |role| and |name| in the subtree of |node|.
  static ui::AXNode* FindAXNode(ui::AXNode* node,
                                ax::mojom::Role role,
                                const std::u16string& name);

  // Gets the AXNode for the given |element| if it's a WebUI element.
  static ui::AXNode* GetAXNode(const ui::TrackedElement* element,
                               ax::mojom::Role role,
                               const std::u16string& name);

  // Gets the AXNodeData for the given |element|. Handles both Views and WebUI.
  static ui::AXNodeData GetAXNodeData(const ui::TrackedElement* element,
                                      ax::mojom::Role role,
                                      const std::u16string& name,
                                      const char* file,
                                      int line);

  // Waits for element to have a size > 1 pixel.
  [[nodiscard]] MultiStep WaitForElementNonzeroSize(ui::ElementIdentifier id);

  // Shorthand for MoveMouseTo that works for both Views and WebUI elements.
  [[nodiscard]] StepBuilder MoveMouseToElement(ui::ElementIdentifier id);

  // Steps to wait for a context menu and then dismiss it.
  [[nodiscard]] MultiStep DismissContextMenu(ui::ElementIdentifier element_id,
                                             ui::ElementIdentifier menu_id);
  // Waits for the specified amount of time.
  [[nodiscard]] StepBuilder DoWaitForTime(base::TimeDelta delay);

  // Wait for a small delay for button layout to settle. Use before clicking
  // on button elements to avoid clicking on the wrong spot due to outdated
  // location information.
  [[nodiscard]] StepBuilder DoWaitForLayout();

  std::unique_ptr<content::ScopedAccessibilityMode> scoped_accessibility_mode_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_ACCESSIBILITY_TEST_H_
