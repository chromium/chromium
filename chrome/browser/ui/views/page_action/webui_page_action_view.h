// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_VIEW_H_

#include <string>

#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/page_action/page_action_view_interface.h"
#include "ui/actions/action_id.h"
#include "ui/views/bubble/bubble_anchor.h"

class IconLabelBubbleView;

namespace page_actions {

class WebUIPageActionControl;

// WebUI-specific implementation of PageActionViewInterface for a single icon.
// Not backed by a traditional views::View object; instead bridges C++ calls
// expecting a PageActionViewInterface (e.g. bubble anchoring, tooltips, and
// visibility changes) to the WebUI front-end via WebUIPageActionControl and
// ToolbarUIService.
class WebUIPageActionView : public PageActionViewInterface {
 public:
  WebUIPageActionView(actions::ActionId action_id,
                      WebUIPageActionControl& owner);
  WebUIPageActionView(const WebUIPageActionView&) = delete;
  WebUIPageActionView& operator=(const WebUIPageActionView&) = delete;
  ~WebUIPageActionView() override;

  // PageActionViewInterface:
  views::BubbleAnchor GetBubbleAnchor() override;
  std::u16string GetTooltipText() const override;
  std::u16string GetAccessibleName() const override;
  void SetVisible(bool visible) override;
  IconLabelBubbleView* GetIconLabelBubbleViewNotMigrated() override;

  actions::ActionId action_id() const { return action_id_; }

 private:
  const actions::ActionId action_id_;
  const raw_ref<WebUIPageActionControl> owner_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_WEBUI_PAGE_ACTION_VIEW_H_
