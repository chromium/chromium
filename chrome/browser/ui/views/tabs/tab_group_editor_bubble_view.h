// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"

class TabController;

namespace gfx {
class Size;
}

class ColorPickerView;

// A dialog for changing a tab group's visual parameters.
class TabGroupEditorBubbleView : public views::BubbleDialogDelegateView {
 public:
  // Shows the editor for |group|. Returns an *unowned* pointer to the
  // bubble's widget.
  static views::Widget* Show(TabGroupHeader* anchor_view,
                             TabController* tab_controller,
                             TabGroupId group);

  // views::BubbleDialogDelegateView:
  gfx::Size CalculatePreferredSize() const override;
  ui::ModalType GetModalType() const override;
  views::View* GetInitiallyFocusedView() override;

 private:
  TabGroupEditorBubbleView(TabGroupHeader* anchor_view,
                           TabController* tab_controller,
                           TabGroupId group);
  ~TabGroupEditorBubbleView() override;

  void UpdateGroup();

  TabController* const tab_controller_;
  const TabGroupId group_;

  class TitleFieldController : public views::TextfieldController {
   public:
    explicit TitleFieldController(TabGroupEditorBubbleView* parent)
        : parent_(parent) {}
    ~TitleFieldController() override = default;

    // views::TextfieldController:
    void ContentsChanged(views::Textfield* sender,
                         const base::string16& new_contents) override;

   private:
    TabGroupEditorBubbleView* const parent_;
  };

  TitleFieldController title_field_controller_;

  class ButtonListener : public views::ButtonListener {
   public:
    explicit ButtonListener(TabController* tab_controller,
                            TabGroupHeader* anchor_view,
                            TabGroupId group);

    // views::ButtonListener:
    void ButtonPressed(views::Button* sender, const ui::Event& event) override;

   private:
    TabController* const tab_controller_;
    TabGroupHeader* anchor_view_;
    const TabGroupId group_;
  };

  ButtonListener button_listener_;

  views::Textfield* title_field_;

  ColorPickerView* color_selector_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
