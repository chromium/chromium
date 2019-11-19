// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/arc/mojom/intent_helper.mojom.h"
#include "ui/aura/window_tracker.h"
#include "ui/views/touchui/touch_selection_menu_runner_views.h"

namespace aura {
class Window;
}

// A Chrome OS TouchSelectionMenuRunner implementation that queries ARC++
// for Smart Text Selection actions based on the current text selection. This
// allows the quick menu to show a new contextual action button.
class TouchSelectionMenuRunnerChromeOS
    : public views::TouchSelectionMenuRunnerViews {
 public:
  TouchSelectionMenuRunnerChromeOS();
  ~TouchSelectionMenuRunnerChromeOS() override;

 private:
  // Called asynchronously with the result from the container.
  void OpenMenuWithTextSelectionAction(
      ui::TouchSelectionMenuClient* client,
      const gfx::Rect& anchor_rect,
      const gfx::Size& handle_image_size,
      std::unique_ptr<aura::WindowTracker> tracker,
      std::vector<arc::mojom::TextSelectionActionPtr> actions);

  // Tries to establish connection with ARC to perform text classification. True
  // if a query to ARC was made, false otherwise.
  bool RequestTextSelection(ui::TouchSelectionMenuClient* client,
                            const gfx::Rect& anchor_rect,
                            const gfx::Size& handle_image_size,
                            aura::Window* context);

  // views::TouchSelectionMenuRunnerViews.
  void OpenMenu(ui::TouchSelectionMenuClient* client,
                const gfx::Rect& anchor_rect,
                const gfx::Size& handle_image_size,
                aura::Window* context) override;

  base::WeakPtrFactory<TouchSelectionMenuRunnerChromeOS> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(TouchSelectionMenuRunnerChromeOS);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOUCH_SELECTION_MENU_RUNNER_CHROMEOS_H_
