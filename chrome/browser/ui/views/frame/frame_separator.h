// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_FRAME_SEPARATOR_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_FRAME_SEPARATOR_H_

#include <optional>

#include "base/callback_list.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/separator.h"
#include "ui/views/metadata/view_factory.h"

// Separator which changes color depending on whether the current window is in
// the "paint as active" state.
//
// When `inactive_color_id_` is set, separator will change color and repaint on
// activity change; when not set behaves identically to `views::Separator`.
class FrameSeparator : public views::Separator {
  METADATA_HEADER(FrameSeparator, views::Separator)
 public:
  FrameSeparator();
  ~FrameSeparator() override;

  std::optional<ui::ColorId> GetInactiveColorId() const;
  void SetInactiveColorId(std::optional<ui::ColorId> inactive_color_id);

 protected:
  // views::Separator:
  void AddedToWidget() override;
  void RemovedFromWidget() override;
  SkColor GetForegroundColor() const override;

 private:
  void MaybeUpdateActiveSubscription();

  // The (optional) color to use when drawing the separator in inactive windows.
  // If not set, `color_id_` is always used.
  std::optional<ui::ColorId> inactive_color_id_;
  base::CallbackListSubscription window_active_subscription_;
};

BEGIN_VIEW_BUILDER(VIEWS_EXPORT, FrameSeparator, views::Separator)
VIEW_BUILDER_PROPERTY(std::optional<ui::ColorId>, InactiveColorId)
END_VIEW_BUILDER

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_FRAME_SEPARATOR_H_
