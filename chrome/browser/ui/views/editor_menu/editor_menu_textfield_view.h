// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_TEXTFIELD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_TEXTFIELD_VIEW_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/view.h"

namespace views {

class ImageButton;

}  // namespace views

namespace chromeos::editor_menu {

class EditorMenuViewDelegate;

// EditorMenuTextfieldView consists of a Textfield and an icon. The Textfiled is
// for inputting text. The icon is a right arrow indicate to send.
class EditorMenuTextfieldView : public views::View,
                                public views::TextfieldController {
 public:
  METADATA_HEADER(EditorMenuTextfieldView);

  explicit EditorMenuTextfieldView(EditorMenuViewDelegate* delegate);
  EditorMenuTextfieldView(const EditorMenuTextfieldView&) = delete;
  EditorMenuTextfieldView& operator=(const EditorMenuTextfieldView&) = delete;
  ~EditorMenuTextfieldView() override;

  views::ImageButton* CreateArrowButton(
      const base::RepeatingClosure& button_callback);

  views::ImageButton* arrow_button() { return arrow_button_; }
  views::Textfield* textfield() { return textfield_; }

  // views::View:
  void AddedToWidget() override;
  int GetHeightForWidth(int width) const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::TextfieldController:
  void ContentsChanged(views::Textfield* sender,
                       const std::u16string& new_contents) override;
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;

 private:
  void InitLayout();
  void OnTextfieldArrowButtonPressed();

  // `delegate_` outlives `this`.
  raw_ptr<EditorMenuViewDelegate> delegate_ = nullptr;

  raw_ptr<views::Textfield> textfield_ = nullptr;
  raw_ptr<views::ImageButton> arrow_button_ = nullptr;

  base::WeakPtrFactory<EditorMenuTextfieldView> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_TEXTFIELD_VIEW_H_
