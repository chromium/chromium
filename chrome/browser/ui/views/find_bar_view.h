// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/chrome_views_export.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/layout/box_layout_view.h"

class FindBarHost;
class FindBarMatchCountLabel;

namespace find_in_page {
class FindNotificationDetails;
}

namespace gfx {
class Range;
}

namespace views {
class Painter;
class Separator;
class Textfield;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarView is responsible for drawing the UI controls of the
// FindBar, the find text box, the 'Find' button and the 'Close'
// button. It communicates the user search words to the FindBarHost.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarView : public views::BoxLayoutView,
                    public views::TextfieldController {
  METADATA_HEADER(FindBarView, views::BoxLayoutView)

 public:
  // Element IDs for ui::ElementTracker
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTextField);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kPreviousButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNextButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kLensButtonElementId);

  explicit FindBarView(FindBarHost* host = nullptr);

  FindBarView(const FindBarView&) = delete;
  FindBarView& operator=(const FindBarView&) = delete;

  ~FindBarView() override;

  void SetHost(FindBarHost* host);

  // Accessors for the text and selection displayed in the text box.
  void SetFindTextAndSelectedRange(const std::u16string& find_text,
                                   const gfx::Range& selected_range);
  std::u16string GetFindText() const;
  gfx::Range GetSelectedRange() const;

  // Gets the selected text in the text box.
  std::u16string GetFindSelectedText() const;

  // Gets the match count text displayed in the text box.
  std::u16string GetMatchCountText() const;

  // Updates the label inside the Find text box that shows the ordinal of the
  // active item and how many matches were found.
  void UpdateForResult(const find_in_page::FindNotificationDetails& result,
                       const std::u16string& find_text);

  // Clears the current Match Count value in the Find text box.
  void ClearMatchCount();

  // Claims focus for the text field and selects its contents.
  void FocusAndSelectAll();

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void OnThemeChanged() override;

  // views::TextfieldController:
  bool HandleKeyEvent(views::Textfield* sender,
                      const ui::KeyEvent& key_event) override;
  void OnAfterUserAction(views::Textfield* sender) override;
  void OnAfterPaste() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(LegacyFindInPageTest, AccessibleName);
  const views::ViewAccessibility&
  GetFindBarMatchCountLabelViewAccessibilityForTesting();

  // Starts finding |search_text|.  If the text is empty, stops finding.
  void Find(const std::u16string& search_text);

  // Find the next/previous occurrence of search text when clicking the
  // next/previous button.
  void FindNext(bool reverse = false);
  // End the current find session and close the find bubble.
  void EndFindSession();

  // Updates the appearance for the match count label.
  void UpdateMatchCountAppearance(bool no_match);

  // Returns the color for the icons on the buttons per the current NativeTheme.
  SkColor GetTextColorForIcon();

  // Hides the Lens Overlay entrypoint if search_text is not empty, else, shows
  // the button.
  void UpdateLensButtonVisibility(const std::u16string& search_text);

  // Returns the IDS to use for the Lens message.
  int GetLensOverlayFindBarMessageIds();

  // Returns the IDS to use for the Lens button label.
  int GetLensOverlayFindBarButtonLabelIds();

  // The OS-specific view for the find bar that acts as an intermediary
  // between us and the WebContentsView.
  raw_ptr<FindBarHost> find_bar_host_;

  // Used to detect if the input text, not including the IME composition text,
  // has changed or not.
  std::u16string last_searched_text_;

  // The controls in the window.
  raw_ptr<views::Textfield> find_text_;
  std::unique_ptr<views::Painter> find_text_border_;
  raw_ptr<FindBarMatchCountLabel> match_count_text_;
  raw_ptr<views::Separator> separator_;
  raw_ptr<views::ImageButton> find_previous_button_;
  raw_ptr<views::ImageButton> find_next_button_;
  raw_ptr<views::ImageButton> close_button_;
  raw_ptr<views::BoxLayoutView> lens_entrypoint_container_;
};

BEGIN_VIEW_BUILDER(/* no export */, FindBarView, views::BoxLayoutView)
VIEW_BUILDER_PROPERTY(FindBarHost*, Host)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, FindBarView)

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_VIEW_H_
