// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/ash/magic_boost/magic_boost_constants.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_card_controller.h"
#include "chromeos/components/mahi/public/cpp/mahi_media_app_events_proxy.h"
#include "chromeos/crosapi/mojom/magic_boost.mojom.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

class ApplicationLocaleStorage;
class Profile;

namespace chromeos {

using OptInFeatures = crosapi::mojom::MagicBoostController::OptInFeatures;
using TransitionAction = crosapi::mojom::MagicBoostController::TransitionAction;

// The controller that manages the lifetime of opt-in cards.
// Some functions in this controller are virtual for testing.
class MagicBoostCardController
    : public ReadWriteCardController,
      public chromeos::MahiMediaAppEventsProxy::Observer {
 public:
  // `application_locale_storage` must be non-null and must outlive `this`.
  explicit MagicBoostCardController(
      const ApplicationLocaleStorage* application_locale_storage);
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

  // chromeos::MahiMediaAppEventsProxy::Observer:
  void OnPdfContextMenuShown(const gfx::Rect& anchor) override;
  void OnPdfContextMenuHide() override;

  // Shows/closes Magic Boost opt-in widget.
  virtual void ShowOptInUi(const gfx::Rect& anchor_view_bounds);
  virtual void CloseOptInUi();

  // Shows/closes Magic Boost disclaimer widget.
  void ShowDisclaimerUi(int64_t display_id);
  void CloseDisclaimerUi();

  // The setter and getter of the features that trigger the magic boost opt in
  // card.
  void SetOptInFeature(const OptInFeatures& features);
  const OptInFeatures& GetOptInFeatures() const;

  void SetMagicBoostControllerCrosapiForTesting(
      crosapi::mojom::MagicBoostController* delegate);

  base::WeakPtr<MagicBoostCardController> GetWeakPtr();

  void set_transition_action(TransitionAction action) {
    transition_action_ = action;
  }
  TransitionAction transition_action_for_test() { return transition_action_; }

  views::Widget* opt_in_widget_for_test() { return opt_in_widget_.get(); }

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;

  TransitionAction transition_action_ = TransitionAction::kDoNothing;

  views::UniqueWidgetPtr opt_in_widget_;

  OptInFeatures opt_in_features_;

  base::WeakPtrFactory<MagicBoostCardController> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_ASH_MAGIC_BOOST_MAGIC_BOOST_CARD_CONTROLLER_H_
