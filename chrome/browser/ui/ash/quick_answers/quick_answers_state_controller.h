// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_

#include "ash/public/cpp/session/session_observer.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

namespace ash {

// Provide access of Assistant related prefs and states to the clients.
class QuickAnswersStateController : public SessionObserver {
 public:
  QuickAnswersStateController();

  QuickAnswersStateController(const QuickAnswersStateController&) = delete;
  QuickAnswersStateController& operator=(const QuickAnswersStateController&) =
      delete;

  ~QuickAnswersStateController() override;

 private:
  // SessionObserver:
  void OnFirstSessionStarted() override;

  QuickAnswersState state_;

  ScopedSessionObserver session_observer_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_QUICK_ANSWERS_QUICK_ANSWERS_STATE_CONTROLLER_H_
