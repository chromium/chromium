// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CHIP_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CHIP_BUTTON_H_

#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

class ToolbarChipButton : public ToolbarButton {
  METADATA_HEADER(ToolbarChipButton, ToolbarButton)

 public:
  explicit ToolbarChipButton(PressedCallback callback,
                             absl::optional<Edge> flat_edge = absl::nullopt);
  ToolbarChipButton(const ToolbarChipButton&) = delete;
  const ToolbarChipButton& operator=(const ToolbarChipButton&) = delete;
  ~ToolbarChipButton() override;

  // Gets/sets whether the button should have a flat edge.
  absl::optional<ToolbarButton::Edge> GetFlatEdge() const;
  void SetFlatEdge(absl::optional<ToolbarButton::Edge> flat_edge);

  // Returns the corner radius of `edge` taking into account the button's
  // `flat_edge_`.
  float GetCornerRadiusFor(Edge edge) const override;

 private:
  // ToolbarButton:
  void UpdateColorsAndInsets() override;

  absl::optional<ToolbarButton::Edge> flat_edge_;
};

BEGIN_VIEW_BUILDER(/* no export */, ToolbarChipButton, ToolbarButton)
VIEW_BUILDER_PROPERTY(absl::optional<ToolbarButton::Edge>, FlatEdge)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, ToolbarChipButton)

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_CHIP_BUTTON_H_
