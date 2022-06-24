// Copyright 2022 The Chromium Authors. All rights reserved.
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
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/base/models/combobox_model.h"

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

  std::string GetCurrentFontName(int index);

 protected:
  // ui::Combobox implementation:
  int GetDefaultIndex() const override;
  int GetItemCount() const override;
  std::u16string GetItemAt(int index) const override;
  std::u16string GetDropDownTextAt(int index) const override;

 private:
  // Styled font names for the drop down options in front-end.
  std::vector<std::u16string> font_choices_;
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
        const std::vector<ui::AXNodeID>& content_node_ids) = 0;
    virtual void OnFontNameUpdated(const std::string& new_font_name) = 0;
  };

  ReadAnythingModel();
  ReadAnythingModel(const ReadAnythingModel&) = delete;
  ReadAnythingModel& operator=(const ReadAnythingModel&) = delete;
  ~ReadAnythingModel();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void SetDistilledAXTree(ui::AXTreeUpdate snapshot,
                          std::vector<ui::AXNodeID> content_node_ids);
  void SetSelectedFontIndex(int new_index);

  ReadAnythingFontModel* GetFontModel() { return font_model_.get(); }

 private:
  void NotifyAXTreeDistilled();
  void NotifyFontNameUpdated();

  // State:
  std::string font_name_;
  // TODO(crbug.com/1266555): Use |snapshot_| and |content_node_ids_| to keep
  // scrolls in sync.
  ui::AXTreeUpdate snapshot_;
  std::vector<ui::AXNodeID> content_node_ids_;

  base::ObserverList<Observer> observers_;
  const std::unique_ptr<ReadAnythingFontModel> font_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
