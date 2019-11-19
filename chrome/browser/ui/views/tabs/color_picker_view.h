// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_COLOR_PICKER_VIEW_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/span.h"
#include "base/optional.h"
#include "ui/views/view.h"

namespace views {
class Button;
}

class ColorPickerElementView;

// Lets users pick from a list of colors displayed as circles that can be
// clicked on. Similar to radio buttons, exactly one is selected after the first
// selection is made.
//
// TODO(crbug.com/989174): make this keyboard and screenreader accessible.
class ColorPickerView : public views::View {
 public:
  using ColorSelectedCallback = base::RepeatingCallback<void()>;
  // |colors| should contain the color values and accessible names. There should
  // not be duplicate colors.
  explicit ColorPickerView(
      base::span<const std::pair<SkColor, base::string16>> colors,
      SkColor initial_color,
      ColorSelectedCallback callback);
  ~ColorPickerView() override;

  // After the callback is called, this is guaranteed to never return nullopt.
  base::Optional<SkColor> GetSelectedColor() const;

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
