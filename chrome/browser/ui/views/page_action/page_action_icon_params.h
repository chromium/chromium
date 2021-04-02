// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;
class CommandUpdater;
class ToolbarIconContainerView;

namespace gfx {
class FontList;
}

struct PageActionIconParams {
  PageActionIconParams();
  ~PageActionIconParams();

  std::vector<PageActionIconType> types_enabled;

  // Leaving these params unset will leave the icon default values untouched.
  // TODO(crbug.com/1061634): Make these fields non-optional.
  base::Optional<SkColor> icon_color;
  const gfx::FontList* font_list = nullptr;

  int between_icon_spacing = 0;
  Browser* browser = nullptr;
  CommandUpdater* command_updater = nullptr;
  IconLabelBubbleView::Delegate* icon_label_bubble_delegate = nullptr;
  PageActionIconView::Delegate* page_action_icon_delegate = nullptr;
  // If in the future another class also wants to observe button changes, this
  // type could be an abstract class that simply exposes an ObserveButton()
  // method.
  ToolbarIconContainerView* button_observer = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(PageActionIconParams);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_
