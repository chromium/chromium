// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_
#define CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view.h"

namespace chromeos {

// View that is placed within the widget controlled by
// `ReadWriteCardsUiController`, which will be placed above or below the context
// menu.
class ReadWriteCardsView : public views::View {
  METADATA_HEADER(ReadWriteCardsView, views::View)

 public:
  ReadWriteCardsView();

  ReadWriteCardsView(const ReadWriteCardsView&) = delete;
  ReadWriteCardsView& operator=(const ReadWriteCardsView&) = delete;

  ~ReadWriteCardsView() override;

  void SetContextMenuBounds(const gfx::Rect& context_menu_bounds);

 protected:
  // Updates bounds according to the new bounds of context menu.
  virtual void UpdateBounds() = 0;

  const gfx::Rect& context_menu_bounds() const { return context_menu_bounds_; }

 private:
  // The bounds of the context menu, used by the view to define bounds and
  // layout.
  gfx::Rect context_menu_bounds_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_
