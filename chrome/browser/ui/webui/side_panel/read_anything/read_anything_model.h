// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_

#include <vector>
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/base/models/combobox_model.h"

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

class ReadAnythingModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnFontNameUpdated(const std::string& new_font_name) = 0;
  };

  ReadAnythingModel();
  ReadAnythingModel(const ReadAnythingModel&) = delete;
  ReadAnythingModel& operator=(const ReadAnythingModel&) = delete;
  ~ReadAnythingModel();

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  void SetSelectedFontIndex(int new_index) {
    currently_selected_font_index_ = new_index;
  }

  void NotifyFontNameUpdated() {
    for (Observer& obs : observers_) {
      obs.OnFontNameUpdated(
          font_model_->GetCurrentFontName(currently_selected_font_index_));
    }
  }

  ReadAnythingFontModel* GetFontModel() { return font_model_.get(); }

 private:
  // State:
  int currently_selected_font_index_ = 0;

  base::ObserverList<Observer> observers_;
  const std::unique_ptr<ReadAnythingFontModel> font_model_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_MODEL_H_
