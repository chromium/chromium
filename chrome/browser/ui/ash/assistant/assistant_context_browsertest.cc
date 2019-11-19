// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/ash/assistant/assistant_client.h"
#include "chrome/browser/ui/ash/assistant/assistant_context_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"

namespace chromeos {
namespace assistant {

namespace {

class AssistantStructureWaiter {
 public:
  AssistantStructureWaiter() = default;

  void Wait() { loop_.Run(); }

  std::unique_ptr<ui::AssistantTree> take_structure() {
    return std::move(structure_);
  }

  void ReceiveStructure(ax::mojom::AssistantExtraPtr assistant_extra,
                        std::unique_ptr<ui::AssistantTree> structure) {
    structure_ = std::move(structure);
    loop_.Quit();
  }

 private:
  std::unique_ptr<ui::AssistantTree> structure_;
  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(AssistantStructureWaiter);
};

}  // namespace

class AssistantContextBrowserTest : public InProcessBrowserTest {
 public:
  AssistantContextBrowserTest() = default;
  ~AssistantContextBrowserTest() override = default;

 protected:
  std::unique_ptr<ui::AssistantTree> GetAssistantStructure(
      const std::string& html) {
    GURL url("data:text/html," + html);
    ui_test_utils::NavigateToURL(browser(), url);
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();

    AssistantStructureWaiter waiter;
    RequestAssistantStructureForWebContentsForTesting(
        web_contents,
        base::BindOnce(&AssistantStructureWaiter::ReceiveStructure,
                       base::Unretained(&waiter)));
    waiter.Wait();
    return waiter.take_structure();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantContextBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AssistantContextBrowserTest,
                       AssistantStructurePositionTest) {
  auto assistant_tree = GetAssistantStructure(
      "<div style='position:absolute;width:200px;height:200px'>"
      "<p style='position:absolute;top:20px;left:20px;margin:0'>Hello</p>"
      "</div>");
  ASSERT_TRUE(assistant_tree);

  ui::AssistantNode* root = assistant_tree->nodes[0].get();

  ASSERT_EQ(root->children_indices.size(), 1ul);
  ui::AssistantNode* child =
      assistant_tree->nodes[root->children_indices[0]].get();

  ui::AssistantNode* grad_child =
      assistant_tree->nodes[child->children_indices[0]].get();
  ASSERT_EQ(base::UTF16ToUTF8(grad_child->text), "Hello");
  ASSERT_EQ(grad_child->rect.x(), 20);
  ASSERT_EQ(grad_child->rect.y(), 20);
}

}  // namespace assistant
}  // namespace chromeos
