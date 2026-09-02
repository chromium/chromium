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
class TrackedElement;
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

  bool GetVisible() const;
  bool IsChipVisible() const;
  bool IsIconVisible() const;
  bool IsLabelVisible() const;
  bool IsAtMinimumSize() const;
  bool IsIconCentered() const;
  bool IsAnimating() const;
  bool HasFocus() const;
  bool HasIconHighlight() const;
  std::u16string GetText() const;
  std::u16string GetTooltipText() const;
  std::u16string GetAccessibleName() const;
  ui::ImageModel GetImage() const;
  ui::TrackedElement* GetElement() const;
  page_actions::PageActionView* view() const;
  std::optional<size_t> GetIndex() const;
  void FinishAnimation() const;
  void Click(page_actions::PageActionTrigger trigger =
                 page_actions::PageActionTrigger::kMouse);
  void SetSuppressionThreshold(base::TimeDelta threshold);

 private:
  page_actions::PageActionViewInterface* GetInterface() const;
  const page_actions::PageActionModelInterface* GetModel() const;
  page_actions::PageActionView* GetPageActionView() const;
  page_actions::WebUIPageActionControl* GetWebUIPageActionControl() const;
  bool EvaluateWebUI(std::string_view element_predicate_js) const;
  ui::TrackedElementWebUI* GetTrackedElement() const;
  views::View* GetView() const;
  content::WebContents* GetWebContents() const;

  raw_ptr<BrowserWindowInterface> browser_;
  actions::ActionId action_id_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_TEST_SUPPORT_PAGE_ACTION_TEST_ACCESSOR_H_
