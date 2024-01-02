// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_model_host.h"

#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

SidePanelModelHost::SidePanelModelHost(std::unique_ptr<SidePanelModel> model)
    : model_(std::move(model)) {
  // TODO(pbos): This needs vertical scroll.
  SetOrientation(views::BoxLayout::Orientation::kVertical);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart);
  for (auto& card : model_->cards()) {
    AddCard(card.get());
  }
}

void SidePanelModelHost::AddCard(ui::DialogModelSection* card) {
  AddChildView(views::DialogModelSectionHost::Create(card));
}

SidePanelModelHost::~SidePanelModelHost() {
  // Make sure we don't outlive our children, who would have dangling pointers
  // to `model_`.
  RemoveAllChildViews();
}
