// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/performance_controls/performance_side_panel_model.h"

#include "chrome/browser/ui/views/side_panel/side_panel_model.h"
#include "ui/base/models/dialog_model_field.h"

std::unique_ptr<SidePanelModel> GetPerformanceSidePanelModel() {
  return SidePanelModel::Builder()
      .AddCard(ui::DialogModelSection::Builder().Build())
      .AddCard(ui::DialogModelSection::Builder()
                   .AddParagraph(ui::DialogModelLabel(
                                     u"See the factors affecting your browsing "
                                     u"experience."),
                                 u"Browser performance")
                   .Build())
      .AddCard(
          ui::DialogModelSection::Builder()
              .AddParagraph(
                  ui::DialogModelLabel(u"Hello from over here next card!!."),
                  u"More card!")
              .AddTextfield(ui::ElementIdentifier(), u"Cool beans?", u"Yes")
              .Build())
      .Build();
}
