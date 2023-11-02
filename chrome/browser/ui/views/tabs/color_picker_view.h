// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "components/tab_groups/tab_group_color.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {
class Button;
class BubbleDialogDelegateView;
}

class ColorPickerElementView;

// Lets users pick from a list of colors displayed as circles that can be
// clicked on. Similar to radio buttons, exactly one is selected after the first
// selection is made.
class ColorPickerView : public views::View {
 public:
  METADATA_HEADER(ColorPickerView);

  using ColorSelectedCallback = base::RepeatingCallback<void()>;

  // |colors| should contain the color values and accessible names. There should
  // not be duplicate colors.
  explicit ColorPickerView(const views::BubbleDialogDelegateView* bubble_view,
                           const TabGroupEditorBubbleView::Colors& colors,
                           tab_groups::TabGroupColorId initial_color_id,
                           ColorSelectedCallback callback);

  ~ColorPickerView() override;

  // Returns the index of the selected element, if any.
  // After the callback is called, this is guaranteed to never return nullopt.
  absl::optional<int> GetSelectedElement() const;

  // views::View:
  views::View* GetSelectedViewForGroup(int group) override;

  views::Button* GetElementAtIndexForTesting(int index);

 private:
  // Handles the selection of a particular color. This is passed as a callback
  // to the views representing each color.
  void OnColorSelected(ColorPickerElementView* element);

  // Called each time the color selection changes.
  ColorSelectedCallback callback_;

  std::vector<ColorPickerElementView*> elements_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
