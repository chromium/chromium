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
#include "ui/base/models/image_model.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace page_actions {
class PageActionModelInterface;
class PageActionView;
class PageActionViewInterface;
class WebUIPageActionControl;
}  // namespace page_actions

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
  bool HasFocus();
  bool HasIconHighlight();
  std::u16string GetText();
  std::u16string GetTooltipText();
  std::u16string GetAccessibleName();
  ui::ImageModel GetImage();
  void Click(page_actions::PageActionTrigger trigger =
                 page_actions::PageActionTrigger::kMouse);
  void SetSuppressionThreshold(base::TimeDelta threshold);

 private:
  page_actions::PageActionViewInterface* GetInterface();
  const page_actions::PageActionModelInterface* GetModel();
  page_actions::PageActionView* GetPageActionView();
  page_actions::WebUIPageActionControl* GetWebUIPageActionControl();
  bool EvaluateWebUI(std::string_view element_predicate_js);
  ui::TrackedElementWebUI* GetTrackedElement();
  views::View* GetView();
  content::WebContents* GetWebContents();

  raw_ptr<BrowserWindowInterface> browser_;
  actions::ActionId action_id_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_ACCESSOR_H_
