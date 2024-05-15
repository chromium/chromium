// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_

#include "ui/views/widget/unique_widget_ptr.h"

namespace views {
class Widget;
}  // namespace views

namespace chromeos {

// The controller that manages the lifetime of opt-in and disclaimer widgets.
class MagicBoostController {
 public:
  MagicBoostController(const MagicBoostController&) = delete;
  MagicBoostController& operator=(const MagicBoostController&) = delete;

  static MagicBoostController* Get();

  // Shows Magic Boost opt-in widget.
  void ShowOptInUi() {}

  // Shows Magic Boost disclaimer widget.
  void ShowDisclaimerUi();

  // For testing.
  views::Widget* disclaimer_widget_for_test() {
    return disclaimer_widget_.get();
  }

 protected:
  friend class base::NoDestructor<MagicBoostController>;

  MagicBoostController();
  ~MagicBoostController();

 private:
  views::UniqueWidgetPtr disclaimer_widget_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
