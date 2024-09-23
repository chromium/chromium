// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/pass_key.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"

class SidePanelModel;

class SidePanelModel final {
 public:
  class Builder final {
   public:
    Builder();
    Builder(const Builder&) = delete;
    Builder& operator=(const Builder&) = delete;
    ~Builder();

    [[nodiscard]] std::unique_ptr<SidePanelModel> Build();

    Builder& AddCard(std::unique_ptr<ui::DialogModelSection> card) {
      model_->AddCard(std::move(card));
      return *this;
    }

   private:
    std::unique_ptr<SidePanelModel> model_;
  };

  explicit SidePanelModel(base::PassKey<SidePanelModel::Builder>);

  SidePanelModel(const SidePanelModel&) = delete;
  SidePanelModel& operator=(const SidePanelModel&) = delete;

  ~SidePanelModel();

  void AddCard(std::unique_ptr<ui::DialogModelSection> card);

  const std::vector<std::unique_ptr<ui::DialogModelSection>>& cards() const {
    return cards_;
  }

 private:
  std::vector<std::unique_ptr<ui::DialogModelSection>> cards_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_SIDE_PANEL_MODEL_H_
