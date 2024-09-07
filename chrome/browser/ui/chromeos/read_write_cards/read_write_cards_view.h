// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_
#define CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ViewShadow;
}  // namespace views

namespace chromeos {

class ReadWriteCardsUiController;

// View that is placed within the widget controlled by
// `ReadWriteCardsUiController`, which will be placed above or below the context
// menu.
class ReadWriteCardsView : public views::View {
  METADATA_HEADER(ReadWriteCardsView, views::View)

 public:
  explicit ReadWriteCardsView(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);

  ReadWriteCardsView(const ReadWriteCardsView&) = delete;
  ReadWriteCardsView& operator=(const ReadWriteCardsView&) = delete;

  ~ReadWriteCardsView() override;

  void SetContextMenuBounds(const gfx::Rect& context_menu_bounds);

  const gfx::Rect& context_menu_bounds_for_test() const {
    return context_menu_bounds_;
  }

 protected:
  // Updates bounds according to the new bounds of context menu.
  // Use `LayoutManager` for updating bounds in the new code. This is for
  // QuickAnswers UI only.
  // TODO(b/331271987): Remove this function once quick answer views fully adopt
  // `LayoutManager`.
  virtual void UpdateBoundsForQuickAnswers();

  // views::View:
  void AddedToWidget() override;

  const gfx::Rect& context_menu_bounds() const { return context_menu_bounds_; }

 private:
  // The bounds of the context menu, used by the view to define bounds and
  // layout.
  gfx::Rect context_menu_bounds_;

  std::unique_ptr<views::ViewShadow> view_shadow_;

  const raw_ref<chromeos::ReadWriteCardsUiController>
      read_write_cards_ui_controller_;
};

BEGIN_VIEW_BUILDER(/* no export */, ReadWriteCardsView, views::View)
END_VIEW_BUILDER

}  // namespace chromeos

DEFINE_VIEW_BUILDER(/* no export */, chromeos::ReadWriteCardsView)

#endif  // CHROME_BROWSER_UI_CHROMEOS_READ_WRITE_CARDS_READ_WRITE_CARDS_VIEW_H_
