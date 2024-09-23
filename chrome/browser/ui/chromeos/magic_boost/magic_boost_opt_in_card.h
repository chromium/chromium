// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class Label;
class MdTextButton;
class UniqueWidgetPtr;
}  // namespace views

namespace chromeos {

class MagicBoostCardController;

// The Magic Boost opt-in card view.
class MagicBoostOptInCard : public chromeos::editor_menu::PreTargetHandlerView {
  METADATA_HEADER(MagicBoostOptInCard,
                  chromeos::editor_menu::PreTargetHandlerView)

 public:
  explicit MagicBoostOptInCard(MagicBoostCardController* controller);
  MagicBoostOptInCard(const MagicBoostOptInCard&) = delete;
  MagicBoostOptInCard& operator=(const MagicBoostOptInCard&) = delete;
  ~MagicBoostOptInCard() override;

  // Creates a widget that contains a `MagicBoostOptInCard`, configured with the
  // given `anchor_view_bounds`.
  static views::UniqueWidgetPtr CreateWidget(
      MagicBoostCardController* controller,
      const gfx::Rect& anchor_view_bounds);

  // Returns the host widget's name.
  static const char* GetWidgetName();

  // Updates the bounds of the widget to the given `anchor_view_bounds`.
  void UpdateWidgetBounds(const gfx::Rect& anchor_view_bounds);

  // views::View:
  void RequestFocus() override;

  // Returns the host widget's name.
  static const char* GetWidgetNameForTest();

 private:
  // Button callbacks.
  void OnPrimaryButtonPressed();
  void OnSecondaryButtonPressed();

  raw_ptr<MagicBoostCardController> controller_ = nullptr;

  // Owned by the views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::Label> body_label_ = nullptr;
  raw_ptr<views::MdTextButton> secondary_button_ = nullptr;

  base::WeakPtrFactory<MagicBoostOptInCard> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_OPT_IN_CARD_H_
