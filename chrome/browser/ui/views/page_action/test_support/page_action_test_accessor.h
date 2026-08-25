// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_ACCESSOR_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_ACCESSOR_H_

#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/page_action/page_action_triggers.h"
#include "ui/actions/action_id.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace ui {
class TrackedElementWebUI;
}

namespace views {
class View;
}

namespace page_actions {

class PageActionTestAccessor {
 public:
  PageActionTestAccessor(BrowserWindowInterface* browser,
                         actions::ActionId action_id);
  PageActionTestAccessor(const PageActionTestAccessor&) = default;
  PageActionTestAccessor& operator=(const PageActionTestAccessor&) = default;
  ~PageActionTestAccessor();

  bool GetVisible();
  bool IsChipVisible();
  bool IsIconVisible();
  bool IsAnimating();
  std::u16string GetText();
  void Click(page_actions::PageActionTrigger trigger =
                 page_actions::PageActionTrigger::kMouse);

 private:
  bool EvaluateWebUI(std::string_view element_predicate_js);
  ui::TrackedElementWebUI* GetTrackedElement();
  views::View* GetView();
  content::WebContents* GetWebContents();

  raw_ptr<BrowserWindowInterface> browser_;
  actions::ActionId action_id_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_ACCESSOR_H_
