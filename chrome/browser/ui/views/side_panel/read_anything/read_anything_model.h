// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/common/accessibility/read_anything.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/models/combobox_model.h"
#include "ui/color/color_id.h"

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingFontModel
//
//  A class that stores the data for the font combobox.
//  This class is owned by the ReadAnythingModel and has the same lifetime as
//  the browser.
//
class ReadAnythingFontModel : public ui::ComboboxModel {
 public:
  ReadAnythingFontModel();
  ReadAnythingFontModel(const ReadAnythingFontModel&) = delete;
  ReadAnythingFontModel& operator=(const ReadAnythingFontModel&) = delete;
  ~ReadAnythingFontModel() override;

  std::string GetFontNameAt(size_t index);
  bool IsValidFontName(const std::string& font_name);
  bool IsValidFontIndex(size_t index);
  void SetDefaultIndexFromPrefsFontName(std::string prefs_font_name);
  std::string GetLabelFontListAt(size_t index);

 protected:
  // ui::Combobox implementation:
  absl::optional<size_t> GetDefaultIndex() const override;
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownTextAt(size_t index) const override;

 private:
  // Styled font names for the drop down options in front-end.
  std::vector<std::u16string> font_choices_;

  // Default index for drop down, either zero or populated from prefs.
  size_t default_index_ = 0;
};

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingColorsModel
//
//  A class that stores the data for the colors combobox.
//  This class is owned by the ReadAnythingModel and has the same lifetime as
//  the browser.
//
class ReadAnythingColorsModel : public ui::ComboboxModel {
 public:
  ReadAnythingColorsModel();
  ReadAnythingColorsModel(const ReadAnythingColorsModel&) = delete;
  ReadAnythingColorsModel& operator=(const ReadAnythingColorsModel&) = delete;
  ~ReadAnythingColorsModel() override;

  // Simple struct to hold the various colors to keep code cleaner.
  struct ColorInfo {
    // The name of the colors, e.g. Default, Light, Dark.
    std::u16string name;

    // The resources value/identifier for the icon image asset.
    int icon_asset;

    // The foreground color, used for text and icon hints.
    SkColor foreground;

    // The background color, used for text background.
    SkColor background;

    // The foreground color as a ColorId, used for separators.
    ui::ColorId foreground_color_id;
  };

  bool IsValidColorsIndex(size_t index);
  void SetDefaultColorsIndexFromPref(size_t index);
  ColorInfo& GetColorsAt(size_t index);
  ui::ColorId GetForegroundColorId(size_t index);

  // Simple pass-through method so Init can set the starting state colors.
  size_t GetStartingStateIndex() { return GetDefaultIndex().value(); }

 protected:
  // ui::Combobox implementation:
  absl::optional<size_t> GetDefaultIndex() const override;
  size_t GetItemCount() const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ImageModel GetDropDownIconAt(size_t index) const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownTextAt(size_t index) const override;

 private:
  // Individual combobox choices for colors presented in front-end.
  std::vector<ColorInfo> colors_choices_;

  // Default index for drop down, either zero or populated from prefs.
  size_t default_index_ = 0;
};

//////////////////////////////////////////////////////////////////////////////
// ReadAnythingLineSpacingModel
//
//  A class that stores the data for the colors combobox.
//  This class is owned by the ReadAnythingModel and has the same lifetime as
//  the browser.
//
class ReadAnythingLineSpacingModel : public ui::ComboboxModel {
 public:
  ReadAnythingLineSpacingModel();
  ReadAnythingLineSpacingModel(const ReadAnythingLineSpacingModel&) = delete;
  ReadAnythingLineSpacingModel& operator=(const ReadAnythingLineSpacingModel&) =
      delete;
  ~ReadAnythingLineSpacingModel() override;

  bool IsValidLineSpacingIndex(size_t index);
  void SetDefaultLineSpacingIndexFromPref(size_t index);
  read_anything::mojom::Spacing GetLineSpacingAt(size_t index);

  // Simple pass-through method so Init can set the starting state.
  size_t GetStartingStateIndex() { return GetDefaultIndex().value(); }

 protected:
  // ui::Combobox implementation:
  absl::optional<size_t> GetDefaultIndex() const override;
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  ui::ImageModel GetDropDownIconAt(size_t index) const override;
  std::u16string GetDropDownTextAt(size_t index) const override;

 private:
  // Names for the drop down options in front-end.
  std::vector<read_anything::mojom::Spacing> lines_choices_;

  // Default index for drop down, either one or populated from prefs.
  size_t default_index_ = 1;

  // Display names for line spacing choices
  std::u16string GetLineSpacingName(
      read_anything::mojom::Spacing line_spacing) const;
};

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingLetterSpacingModel
//
//  A class that stores the data for the letter spacing combobox.
//  This class is owned by the ReadAnythingModel and has the same lifetime as
//  the browser.
//
class ReadAnythingLetterSpacingModel : public ui::ComboboxModel {
 public:
  ReadAnythingLetterSpacingModel();
  ReadAnythingLetterSpacingModel(const ReadAnythingLetterSpacingModel&) =
      delete;
  ReadAnythingLetterSpacingModel& operator=(
      const ReadAnythingLetterSpacingModel&) = delete;
  ~ReadAnythingLetterSpacingModel() override;

  bool IsValidLetterSpacingIndex(size_t index);
  void SetDefaultLetterSpacingIndexFromPref(size_t index);
  read_anything::mojom::Spacing GetLetterSpacingAt(size_t index);

 protected:
  // ui::Combobox implementation:
  absl::optional<size_t> GetDefaultIndex() const override;
  size_t GetItemCount() const override;
  ui::ImageModel GetIconAt(size_t index) const override;
  std::u16string GetItemAt(size_t index) const override;
  std::u16string GetDropDownTextAt(size_t index) const override;
  ui::ImageModel GetDropDownIconAt(size_t index) const override;

 private:
  // Letter spacing choices for the drop down options in front-end.
  std::vector<read_anything::mojom::Spacing> letter_spacing_choices_;

  // Default index for drop down, either one (normal spacing) or populated from
  // prefs.
  size_t default_index_ = 1;

  // Display names for each letter spacing choice
  std::u16string GetLetterSpacingName(
      read_anything::mojom::Spacing letter_spacing) const;
};
///////////////////////////////////////////////////////////////////////////////
// ReadAnythingModel
//
//  A class that stores data for the Read Anything feature.
//  This class is owned by the ReadAnythingCoordinator and has the same lifetime
//  as the browser.
//
class ReadAnythingModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnAXTreeDistilled(
        const ui::AXTreeUpdate& snapshot,
        const std::vector<ui::AXNodeID>& content_node_ids) {}
    virtual void OnReadAnythingThemeChanged(
        read_anything::mojom::ReadAnythingThemePtr new_theme) = 0;
  };

  ReadAnythingModel();
  ReadAnythingModel(const ReadAnythingModel&) = delete;
  ReadAnythingModel& operator=(const ReadAnythingModel&) = delete;
  ~ReadAnythingModel();

  void Init(std::string& font_name,
            double font_scale,
            read_anything::mojom::Colors colors,
            read_anything::mojom::Spacing line_spacing,
            read_anything::mojom::Spacing letter_spacing);

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void SetDistilledAXTree(ui::AXTreeUpdate snapshot,
                          std::vector<ui::AXNodeID> content_node_ids);

  void SetSelectedFontByIndex(size_t new_index);
  void DecreaseTextSize();
  void IncreaseTextSize();
  void SetSelectedColorsByIndex(size_t new_index);
  void SetSelectedLineSpacingByIndex(size_t new_index);
  void SetSelectedLetterSpacingByIndex(size_t new_index);

  ReadAnythingFontModel* GetFontModel() { return font_model_.get(); }
  double GetFontScale() { return font_scale_; }
  ReadAnythingColorsModel* GetColorsModel() { return colors_model_.get(); }
  ui::ColorId GetForegroundColorId();
  ReadAnythingLineSpacingModel* GetLineSpacingModel() {
    return line_spacing_model_.get();
  }
  ReadAnythingLetterSpacingModel* GetLetterSpacingModel() {
    return letter_spacing_model_.get();
  }

 private:
  void NotifyAXTreeDistilled();
  void NotifyThemeChanged();

  // State:

  // Members of read_anything::mojom::ReadAnythingTheme:
  std::string font_name_;
  SkColor foreground_color_;
  SkColor background_color_;

  // A scale multiplier for font size (internal use only, not shown to user).
  float font_scale_;

  read_anything::mojom::Spacing line_spacing_;
  read_anything::mojom::Spacing letter_spacing_;

  // Currently selected index for colors combobox
  int colors_combobox_index_;

  // TODO(crbug.com/1266555): Use |snapshot_| and |content_node_ids_| to keep
  // scrolls in sync.
  ui::AXTreeUpdate snapshot_;
  std::vector<ui::AXNodeID> content_node_ids_;

  base::ObserverList<Observer> observers_;
  const std::unique_ptr<ReadAnythingFontModel> font_model_;
  const std::unique_ptr<ReadAnythingColorsModel> colors_model_;
  const std::unique_ptr<ReadAnythingLineSpacingModel> line_spacing_model_;
  const std::unique_ptr<ReadAnythingLetterSpacingModel> letter_spacing_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
