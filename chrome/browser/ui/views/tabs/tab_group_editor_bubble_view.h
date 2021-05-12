// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_

#include <string>

#include "base/optional.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/metadata/metadata_header_macros.h"

class Browser;

namespace tab_groups {
enum class TabGroupColorId;
class TabGroupId;
}  // namespace tab_groups

class ColorPickerView;
class TabGroupHeader;

// A dialog for changing a tab group's visual parameters.
class TabGroupEditorBubbleView : public views::BubbleDialogDelegateView {
 public:
  METADATA_HEADER(TabGroupEditorBubbleView);

  static constexpr int TAB_GROUP_HEADER_CXMENU_NEW_TAB_IN_GROUP = 13;
  static constexpr int TAB_GROUP_HEADER_CXMENU_UNGROUP = 14;
  static constexpr int TAB_GROUP_HEADER_CXMENU_CLOSE_GROUP = 15;
  static constexpr int TAB_GROUP_HEADER_CXMENU_MOVE_GROUP_TO_NEW_WINDOW = 16;
  static constexpr int TAB_GROUP_HEADER_CXMENU_FEEDBACK = 17;

  using Colors =
      std::vector<std::pair<tab_groups::TabGroupColorId, std::u16string>>;

  // Shows the editor for |group|. Returns a *non-owning* pointer to the
  // bubble's widget.
  static views::Widget* Show(
      const Browser* browser,
      const tab_groups::TabGroupId& group,
      TabGroupHeader* header_view,
      base::Optional<gfx::Rect> anchor_rect = base::nullopt,
      // If not provided, will be set to |header_view|.
      views::View* anchor_view = nullptr,
      bool stop_context_menu_propagation = false);

  // views::BubbleDialogDelegateView:
  views::View* GetInitiallyFocusedView() override;
  gfx::Rect GetAnchorRect() const override;

 private:
  TabGroupEditorBubbleView(const Browser* browser,
                           const tab_groups::TabGroupId& group,
                           views::View* anchor_view,
                           base::Optional<gfx::Rect> anchor_rect,
                           TabGroupHeader* header_view,
                           bool stop_context_menu_propagation);
  ~TabGroupEditorBubbleView() override;

  void UpdateGroup();

  void NewTabInGroupPressed();
  void UngroupPressed(TabGroupHeader* header_view);
  void CloseGroupPressed();
  void MoveGroupToNewWindowPressed();
  void SendFeedbackPressed();

  void OnBubbleClose();

  const Browser* const browser_;
  const tab_groups::TabGroupId group_;

  class TitleFieldController : public views::TextfieldController {
   public:
    explicit TitleFieldController(TabGroupEditorBubbleView* parent)
        : parent_(parent) {}
    ~TitleFieldController() override = default;

    // views::TextfieldController:
    void ContentsChanged(views::Textfield* sender,
                         const std::u16string& new_contents) override;
    bool HandleKeyEvent(views::Textfield* sender,
                        const ui::KeyEvent& key_event) override;

   private:
    TabGroupEditorBubbleView* const parent_;
  };

  TitleFieldController title_field_controller_;

  class TitleField : public views::Textfield {
   public:
    METADATA_HEADER(TitleField);
    explicit TitleField(bool stop_context_menu_propagation)
        : stop_context_menu_propagation_(stop_context_menu_propagation) {}
    ~TitleField() override = default;

    // views::Textfield:
    void ShowContextMenu(const gfx::Point& p,
                         ui::MenuSourceType source_type) override;

   private:
    // Whether the context menu should be hidden the first time it shows.
    // Needed because there is no easy way to stop the propagation of a
    // ShowContextMenu event, which is sometimes used to open the bubble
    // itself.
    bool stop_context_menu_propagation_;
  };

  TitleField* title_field_;

  Colors colors_;
  ColorPickerView* color_selector_;

  // If true will use the |anchor_rect_| provided in the constructor, otherwise
  // fall back to using the anchor view bounds.
  const bool use_set_anchor_rect_;

  // Creates the set of tab group colors to display and returns the color that
  // is initially selected.
  tab_groups::TabGroupColorId InitColorSet();

  std::u16string title_at_opening_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_EDITOR_BUBBLE_VIEW_H_
