// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
#define CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_

namespace chromeos {

// The controller that manages the lifetime of opt-in and disclaimer widgets.
class MagicBoostController {
 public:
  MagicBoostController(const MagicBoostController&) = delete;
  MagicBoostController& operator=(const MagicBoostController&) = delete;

  ~MagicBoostController();

  static MagicBoostController* Get();

  // Shows Magic Boost opt-in widget.
  void ShowOptInUi() {}

  // Shows Magic Boost disclaimer widget.
  void ShowDisclaimerUi() {}

 protected:
  MagicBoostController();
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_CHROMEOS_MAGIC_BOOST_MAGIC_BOOST_CONTROLLER_H_
