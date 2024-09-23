// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
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
  PageActionIconParams(const PageActionIconParams&) = delete;
  PageActionIconParams& operator=(const PageActionIconParams&) = delete;
  ~PageActionIconParams();

  std::vector<PageActionIconType> types_enabled;

  // Leaving these params unset will leave the icon default values untouched.
  // TODO(crbug.com/40679715): Make these fields non-optional.
  std::optional<SkColor> icon_color;
  raw_ptr<const gfx::FontList> font_list = nullptr;

  int between_icon_spacing = 0;
  raw_ptr<Browser> browser = nullptr;
  raw_ptr<CommandUpdater> command_updater = nullptr;
  raw_ptr<IconLabelBubbleView::Delegate> icon_label_bubble_delegate = nullptr;
  raw_ptr<PageActionIconView::Delegate> page_action_icon_delegate = nullptr;
  // If in the future another class also wants to observe button changes, this
  // type could be an abstract class that simply exposes an ObserveButton()
  // method.
  raw_ptr<ToolbarIconContainerView> button_observer = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_PAGE_ACTION_ICON_PARAMS_H_
