// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HOTKEY_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HOTKEY_BUBBLE_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}

namespace omnibox_everywhere {

// A bubble dialog view that presents preset keyboard shortcuts for Loomnibox
// FRE. Floating outside the parent window, it avoids WebUI widget clipping
// while matching the Chrome Design System Figma specifications.
class OmniboxEverywhereHotkeyBubbleView : public views::BubbleDialogDelegate,
                                          public views::WidgetObserver {
 public:
  using SelectHotkeyCallback =
      base::RepeatingCallback<void(const std::string&)>;

  static void Show(views::Widget* parent_widget,
                   const gfx::Rect& anchor_rect,
                   SelectHotkeyCallback on_select_hotkey,
                   base::OnceClosure on_closed = base::NullCallback());

  static void CloseIfOpen();

  static views::Widget* GetWidgetForTesting();
  static OmniboxEverywhereHotkeyBubbleView* GetCurrentForTesting();

  OmniboxEverywhereHotkeyBubbleView(
      views::Widget* parent_widget,
      const gfx::Rect& anchor_rect,
      SelectHotkeyCallback on_select_hotkey,
      base::OnceClosure on_closed = base::NullCallback());
  OmniboxEverywhereHotkeyBubbleView(const OmniboxEverywhereHotkeyBubbleView&) =
      delete;
  OmniboxEverywhereHotkeyBubbleView& operator=(
      const OmniboxEverywhereHotkeyBubbleView&) = delete;
  ~OmniboxEverywhereHotkeyBubbleView() override;

  // views::BubbleDialogDelegate:
  gfx::Rect GetBubbleBounds() override;

  // views::DialogDelegate:
  bool Cancel() override;
  void WindowClosing() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  base::OnceClosure take_on_closed() { return std::move(on_closed_); }

 private:
  raw_ptr<views::Widget> parent_widget_ = nullptr;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      parent_widget_observation_{this};
  SelectHotkeyCallback on_select_hotkey_;
  base::OnceClosure on_closed_;
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HOTKEY_BUBBLE_VIEW_H_
