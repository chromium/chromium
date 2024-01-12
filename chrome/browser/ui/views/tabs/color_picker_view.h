// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_

#include <optional>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/tabs/tab_group_editor_bubble_view.h"
#include "components/tab_groups/tab_group_color.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {
class Button;
class BubbleDialogDelegateView;
}  // namespace views

class ColorPickerElementView;

// Lets users pick from a list of colors displayed as circles that can be
// clicked on. Similar to radio buttons, exactly one is selected after the first
// selection is made.
class ColorPickerView : public views::View {
  METADATA_HEADER(ColorPickerView, views::View)

 public:
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
  std::optional<int> GetSelectedElement() const;

  // views::View:
  views::View* GetSelectedViewForGroup(int group) override;

  views::Button* GetElementAtIndexForTesting(int index);

 private:
  // Handles the selection of a particular color. This is passed as a callback
  // to the views representing each color.
  void OnColorSelected(ColorPickerElementView* element);

  // Called each time the color selection changes.
  ColorSelectedCallback callback_;

  std::vector<raw_ptr<ColorPickerElementView, VectorExperimental>> elements_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
