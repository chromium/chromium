// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_model.h"

#include <ostream>

#include "base/check.h"

const void* const kSidePanelEntryCardUserDataKey =
    &kSidePanelEntryCardUserDataKey;

SidePanelModel::Builder::Builder()
    : model_(std::make_unique<SidePanelModel>(base::PassKey<Builder>())) {}

SidePanelModel::Builder::~Builder() {
  CHECK(!model_) << "Model should've been built.";
}

std::unique_ptr<SidePanelModel> SidePanelModel::Builder::Build() {
  CHECK(model_);
  return std::move(model_);
}

SidePanelModel::SidePanelModel(base::PassKey<Builder>) {}

SidePanelModel::~SidePanelModel() = default;

void SidePanelModel::AddCard(std::unique_ptr<ui::DialogModelSection> card) {
  cards_.push_back(std::move(card));
}
