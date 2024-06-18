// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/no_destructor.h"
#include "chromeos/components/editor_menu/public/cpp/read_write_card_controller.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "ui/views/widget/unique_widget_ptr.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace mahi {
class MahiPrefsController;
}  // namespace mahi

class Profile;

namespace chromeos {

// The controller that manages the lifetime of opt-in cards.
// Some functions in this controller are virtual for testing.
class MagicBoostCardController : public ReadWriteCardController {
 public:
  MagicBoostCardController();
  MagicBoostCardController(const MagicBoostCardController&) = delete;
  MagicBoostCardController& operator=(const MagicBoostCardController&) = delete;
  ~MagicBoostCardController() override;

  // ReadWriteCardController:
  void OnContextMenuShown(Profile* profile) override;
  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override;
  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override;
  void OnDismiss(bool is_other_command_executed) override;

  // Shows/closes Magic Boost opt-in widget.
  virtual void ShowOptInUi(const gfx::Rect& anchor_view_bounds);
  virtual void CloseOptInUi();

  // Shows/closes Magic Boost disclaimer widget.
  void ShowDisclaimerUi(
      int64_t display_id,
      crosapi::mojom::MagicBoostController::TransitionAction action);

  // Whether the Quick Answers and Mahi features should show the opt in UI.
  virtual bool ShouldQuickAnswersAndMahiShowOptIn();

  // Enables or disables all the features (including Quick Answers, Orca, and
  // Mahi).
  virtual void SetAllFeaturesState(bool enabled);

  // Enables or disables Quick Answers and Mahi.
  virtual void SetQuickAnswersAndMahiFeaturesState(bool enabled);

  // Enables or disables Orca.
  void SetOrcaFeatureState(bool enabled) {}

  bool is_orca_included() { return is_orca_included_; }

  // For testing.
  void SetIsOrcaIncludedForTest(bool include);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  void BindMagicBoostControllerCrosapiForTesting(
      mojo::PendingRemote<crosapi::mojom::MagicBoostController> pending_remote);
#else   // BUILDFLAG(IS_CHROMEOS_ASH)
  void SetMagicBoostControllerCrosapiForTesting(
      crosapi::mojom::MagicBoostController* delegate);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  views::Widget* opt_in_widget_for_test() { return opt_in_widget_.get(); }

 private:
  // If Orca feature is included.
  bool is_orca_included_ = false;

  views::UniqueWidgetPtr opt_in_widget_;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  mojo::Remote<crosapi::mojom::MagicBoostController> remote_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_
