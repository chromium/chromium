// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_TITLE_FIELD_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_TITLE_FIELD_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/views/controls/textfield/textfield.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/callback_list.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#endif

namespace gfx {
class Point;
}

namespace ui {
class TrackedElement;
}

namespace views {
class TextfieldController;
}

// The title text field shared by the tab group editor bubbles
// (TabGroupEditorBubbleView and the ephemeral tab group promotion bubble). It
// optionally swallows the first context menu request (needed when the bubble
// itself was opened from a context menu event) and, on ChromeOS, keeps the
// hosting bubble open while the emoji picker has focus. The hosting bubble is
// resolved from the widget this view lives in, so it can be constructed before
// the bubble exists.
class TabGroupEditorTitleField : public views::Textfield {
  METADATA_HEADER(TabGroupEditorTitleField, views::Textfield)

 public:
  // Creates a title field configured for the editor bubbles: `title` as text,
  // the shared accessible name, placeholder, element identifier and margins.
  // `controller` may be null.
  static std::unique_ptr<TabGroupEditorTitleField> Create(
      views::TextfieldController* controller,
      const std::u16string& title,
      bool stop_context_menu_propagation);

  explicit TabGroupEditorTitleField(bool stop_context_menu_propagation);
  TabGroupEditorTitleField(const TabGroupEditorTitleField&) = delete;
  TabGroupEditorTitleField& operator=(const TabGroupEditorTitleField&) = delete;
  ~TabGroupEditorTitleField() override;

  // views::Textfield:
  void ShowContextMenu(const gfx::Point& p,
                       ui::mojom::MenuSourceType source_type) override;
#if BUILDFLAG(IS_CHROMEOS)
  void ExecuteCommand(int command_id, int event_flags) override;
#endif

 private:
#if BUILDFLAG(IS_CHROMEOS)
  // Ensures the hosting bubble remains open when the emoji picker menu steals
  // focus (crbug.com/485778122). Needed as both are browser-managed UIs in
  // ChromeOS.
  void PreventCloseOnDeactivateForEmoji();
  void OnEmojiPickerClosed(ui::TrackedElement* element);

  std::unique_ptr<views::BubbleDialogDelegate::CloseOnDeactivatePin>
      close_on_deactivate_pin_;
  base::CallbackListSubscription emoji_picker_hidden_subscription_;
#endif

  // Whether the context menu should be hidden the first time it shows. Needed
  // because there is no easy way to stop the propagation of a ShowContextMenu
  // event, which is sometimes used to open the bubble itself.
  bool stop_context_menu_propagation_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_GROUPS_TAB_GROUP_EDITOR_TITLE_FIELD_H_
