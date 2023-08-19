// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_TEST_API_H_
#define CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_TEST_API_H_

#include "base/memory/raw_ptr.h"

namespace chromeos {

class MultitaskButton;
class MultitaskMenuView;
class SplitButtonView;

// Wrapper for MultitaskMenuView that exposes internal state to test
// functions.
class MultitaskMenuViewTestApi {
 public:
  explicit MultitaskMenuViewTestApi(MultitaskMenuView* view);
  MultitaskMenuViewTestApi(const MultitaskMenuViewTestApi&) = delete;
  MultitaskMenuViewTestApi& operator=(const MultitaskMenuViewTestApi&) = delete;
  ~MultitaskMenuViewTestApi();

  SplitButtonView* GetHalfButton();
  MultitaskButton* GetFullButton();
  MultitaskButton* GetFloatButton();

  // The partial button's left/top button normally snaps 2/3 to the left/top,
  // and the right/bottom button normally snaps 1/3 to the right/bottom. The
  // user can use the alt key to toggle to and from reversed state, where the
  // left/top button would snap 1/3 and the right/bottom button would snap 2/3.
  bool GetIsReversed() const;

 private:
  const raw_ptr<MultitaskMenuView> multitask_menu_view_;
};

}  // namespace chromeos

#endif  // CHROMEOS_UI_FRAME_MULTITASK_MENU_MULTITASK_MENU_VIEW_TEST_API_H_
