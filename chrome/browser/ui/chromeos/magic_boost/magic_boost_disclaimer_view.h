// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler_view.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {

class MdTextButton;
class UniqueWidgetPtr;

}  // namespace views

namespace chromeos {

// A bubble style view to show the disclaimer view.
class MagicBoostDisclaimerView
    : public chromeos::editor_menu::PreTargetHandlerView {
  METADATA_HEADER(MagicBoostDisclaimerView,
                  chromeos::editor_menu::PreTargetHandlerView)

 public:
  MagicBoostDisclaimerView();
  MagicBoostDisclaimerView(const MagicBoostDisclaimerView&) = delete;
  MagicBoostDisclaimerView& operator=(const MagicBoostDisclaimerView&) = delete;
  ~MagicBoostDisclaimerView() override;

  // chromeos::editor_menu::PreTargetHandlerView:
  void RequestFocus() override;

  // Creates a widget that contains a `DisclaimerView`, shown in the middle of
  // the screen.
  static views::UniqueWidgetPtr CreateWidget();

  // Returns the host widget's name.
  static const char* GetWidgetName();

 private:
  // Button callbacks.
  void OnAcceptButtonPressed();
  void OnDeclineButtonPressed();

  // Owned by the views hierarchy.
  raw_ptr<views::MdTextButton> accept_button_ = nullptr;

  base::WeakPtrFactory<MagicBoostDisclaimerView> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_DISCLAIMER_VIEW_H_
