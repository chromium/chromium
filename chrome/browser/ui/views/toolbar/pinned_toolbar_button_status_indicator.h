// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_BUTTON_STATUS_INDICATOR_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_BUTTON_STATUS_INDICATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

class PinnedToolbarButtonStatusIndicator : public views::View {
  METADATA_HEADER(PinnedToolbarButtonStatusIndicator, View)

 public:
  PinnedToolbarButtonStatusIndicator(PinnedToolbarButtonStatusIndicator&) =
      delete;
  PinnedToolbarButtonStatusIndicator& operator=(
      const PinnedToolbarButtonStatusIndicator&) = delete;
  ~PinnedToolbarButtonStatusIndicator() override;

  // Create a PinnedToolbarButtonStatusIndicator and adds it to |parent|. The
  // returned status indicator is owned by the |parent|.
  static PinnedToolbarButtonStatusIndicator* Install(View* parent);

  void SetColor(SkColor color);

  void Show();
  void Hide();

 private:
  PinnedToolbarButtonStatusIndicator();

  // View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  std::optional<SkColor> color_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_PINNED_TOOLBAR_BUTTON_STATUS_INDICATOR_H_
