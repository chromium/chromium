// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_accessibility_test.h"

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_web_ui.h"

ToolbarAccessibilityTest::ToolbarAccessibilityTest() = default;
ToolbarAccessibilityTest::~ToolbarAccessibilityTest() = default;

void ToolbarAccessibilityTest::SetUpOnMainThread() {
  InteractiveBrowserTest::SetUpOnMainThread();
}

void ToolbarAccessibilityTest::WaitForInitialWebUI(Browser* browser) {
  if (!browser) {
    browser = this->browser();
  }
  WaitForInitialWebUIToolbar(browser);
}

void ToolbarAccessibilityTest::ConfigureAccessibilityForWebUITest(
    bool use_webui) {
  std::optional<content::AccessibilityNotificationWaiter> load_waiter;
  if (use_webui) {
    content::WebContents* toolbar_webcontents =
        browser()
            ->GetBrowserView()
            .toolbar_button_provider()
            ->GetWebUIToolbarViewForTesting()
            ->GetWebViewForTesting()
            ->GetWebContents();
    ASSERT_TRUE(toolbar_webcontents);
    load_waiter.emplace(toolbar_webcontents, ax::mojom::Event::kLoadComplete);
  }

  bool accessibility_initially_disabled =
      content::BrowserAccessibilityState::GetInstance()
          ->GetAccessibilityMode()
          .is_mode_off();
  scoped_accessibility_mode_ =
      content::BrowserAccessibilityState::GetInstance()
          ->CreateScopedModeForProcess(ui::kAXModeComplete);
  if (load_waiter) {
    if (accessibility_initially_disabled) {
      ASSERT_TRUE(load_waiter->WaitForNotification());
    } else {
      ASSERT_TRUE(base::test::RunUntil([this]() {
        BrowserView* const browser_view =
            BrowserView::GetBrowserViewForBrowser(browser());
        ToolbarButtonProvider* provider =
            browser_view->toolbar_button_provider();
        if (WebUIToolbarWebView* toolbar_web_view =
                provider->GetWebUIToolbarViewForTesting()) {
          return toolbar_web_view->GetWebViewForTesting()
                     ->web_contents()
                     ->GetAccessibilityRootNode() != nullptr;
        }
        return true;
      }));
    }
  }
}

// static
ui::AXNode* ToolbarAccessibilityTest::FindAXNode(ui::AXNode* node,
                                                 ax::mojom::Role role,
                                                 const std::u16string& name) {
  if (node->data().role == role &&
      node->data().GetString16Attribute(ax::mojom::StringAttribute::kName) ==
          name) {
    return node;
  }
  for (ui::AXNode* child : node->children()) {
    if (ui::AXNode* found = FindAXNode(child, role, name)) {
      return found;
    }
  }
  return nullptr;
}

// static
ui::AXNode* ToolbarAccessibilityTest::GetAXNode(
    const ui::TrackedElement* element,
    ax::mojom::Role role,
    const std::u16string& name) {
  if (auto* const webui_el = element->AsA<ui::TrackedElementWebUI>()) {
    ui::AXNode* root =
        webui_el->handler()->web_contents()->GetAccessibilityRootNode();
    return root ? FindAXNode(root, role, name) : nullptr;
  }
  return nullptr;
}

// static
ui::AXNodeData ToolbarAccessibilityTest::GetAXNodeData(
    const ui::TrackedElement* element,
    ax::mojom::Role role,
    const std::u16string& name,
    const char* file,
    int line) {
  if (auto* const view_el = element->AsA<views::TrackedElementViews>()) {
    ui::AXNodeData node_data;
    view_el->view()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
    return node_data;
  } else if (element->AsA<ui::TrackedElementWebUI>()) {
    ui::AXNode* node = GetAXNode(element, role, name);
    if (!node) {
      ADD_FAILURE_AT(file, line) << "Could not find AXNode with name: " << name;
      return {};
    }
    return node->data();
  } else {
    ADD_FAILURE_AT(file, line) << "Unsupported element type";
    return {};
  }
}

ToolbarAccessibilityTest::MultiStep
ToolbarAccessibilityTest::WaitForElementNonzeroSize(ui::ElementIdentifier id) {
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(
      ui::test::PollingElementStateObserver<bool>, kElementSizeGreaterThanOne);
  return Steps(PollElement(kElementSizeGreaterThanOne, id,
                           [](const ui::TrackedElement* el) {
                             auto size = el->GetScreenBounds();
                             return size.width() > 1 && size.height() > 1;
                           }),
               WaitForState(kElementSizeGreaterThanOne, true),
               StopObservingState(kElementSizeGreaterThanOne));
}

ui::InteractionSequence::StepBuilder
ToolbarAccessibilityTest::MoveMouseToElement(ui::ElementIdentifier id) {
  return MoveMouseTo(id, base::BindOnce([](ui::TrackedElement* el) {
                       return el->GetScreenBounds().CenterPoint();
                     }));
}

ToolbarAccessibilityTest::MultiStep
ToolbarAccessibilityTest::DismissContextMenu(ui::ElementIdentifier element_id,
                                             ui::ElementIdentifier menu_id) {
  return Steps(WaitForShow(menu_id),
               IfElement(
                   element_id,
                   [](const ui::TrackedElement* el) {
                     return !!el->AsA<views::TrackedElementViews>();
                   },
                   Then(SendKeyPress(element_id, ui::VKEY_ESCAPE)),
                   Else(WithElement(element_id, [this](ui::TrackedElement* el) {
                     CHECK(el->AsA<ui::TrackedElementWebUI>());
                     std::ignore = ui_test_utils::SendKeyPressSync(
                         browser(), ui::VKEY_ESCAPE, false, false, false,
                         false);
                   }))));
}

ui::InteractionSequence::StepBuilder ToolbarAccessibilityTest::DoWaitForTime(
    base::TimeDelta delay) {
  StepBuilder step = Do(base::BindOnce(
      [](base::TimeDelta delay) {
        // Have to allow nestable tasks to use this within a RunTestSequence()
        // call.
        base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
            FROM_HERE, run_loop.QuitClosure(), delay);
        run_loop.Run();
      },
      delay));
  step.SetDescription("DoWaitForTime()");
  return step;
}

ui::InteractionSequence::StepBuilder
ToolbarAccessibilityTest::DoWaitForLayout() {
  // Wait for a small delay for button layout to settle.
  return DoWaitForTime(base::Milliseconds(100));
}
