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
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything.mojom.h"
#include "ui/base/models/combobox_model.h"

using read_anything::mojom::ContentNodePtr;

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
    virtual void OnFontNameUpdated(const std::string& new_font_name) = 0;
    virtual void OnContentUpdated(
        const std::vector<ContentNodePtr>& content) = 0;
  };

  ReadAnythingModel();
  ReadAnythingModel(const ReadAnythingModel&) = delete;
  ReadAnythingModel& operator=(const ReadAnythingModel&) = delete;
  ~ReadAnythingModel();

  void AddObserver(Observer* obs);
  void RemoveObserver(Observer* obs);

  void SetSelectedFontIndex(int new_index);
  void SetContent(std::vector<ContentNodePtr> content_nodes);

  ReadAnythingFontModel* GetFontModel() { return font_model_.get(); }

 private:
  void NotifyFontNameUpdated();
  void NotifyContentUpdated();

  // State:
  std::string font_name_;
  std::vector<ContentNodePtr> content_nodes_;

  base::ObserverList<Observer> observers_;
  const std::unique_ptr<ReadAnythingFontModel> font_model_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
