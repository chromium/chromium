// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_

#include "base/no_destructor.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

// The controller that manages the lifetime of opt-in and disclaimer widgets.
// Some functions in this controller are virtual for testing.
class MagicBoostController {
 public:
  MagicBoostController(const MagicBoostController&) = delete;
  MagicBoostController& operator=(const MagicBoostController&) = delete;

  static MagicBoostController* Get();

  // Shows/closes Magic Boost opt-in widget.
  virtual void ShowOptInUi(const gfx::Rect& anchor_view_bounds);
  virtual void CloseOptInUi();

  // Shows Magic Boost disclaimer widget.
  void ShowDisclaimerUi();

  // For testing.
  views::Widget* opt_in_widget_for_test() { return opt_in_widget_.get(); }
  views::Widget* disclaimer_widget_for_test() {
    return disclaimer_widget_.get();
  }

  // Closes Magic Boost disclaimer widget.
  void CloseDisclaimerUi() {}

  // Whether the Quick Answers and Mahi features should show the opt in UI.
  virtual bool ShouldQuickAnswersAndMahiShowOptIn();

  // Enables or disables all the features (including Quick Answers, Orca, and
  // Mahi).
  void SetAllFeaturesState(bool enabled);

  // Enables or disables Quick Answers and Mahi.
  void SetQuickAnswersAndMahiFeaturesState(bool enabled);

  // Enables or disables Orca.
  void SetOrcaFeatureState(bool enabled) {}

 protected:
  friend class base::NoDestructor<MagicBoostController>;

  MagicBoostController();
  ~MagicBoostController();

 private:
  views::UniqueWidgetPtr opt_in_widget_;
  views::UniqueWidgetPtr disclaimer_widget_;
};

// Helper class to automatically set and reset the `MagicBoostController` global
// instance for testing.
class ScopedMagicBoostControllerForTesting {
 public:
  explicit ScopedMagicBoostControllerForTesting(
      MagicBoostController* controller_for_testing);
  ~ScopedMagicBoostControllerForTesting();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
