// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_

#include "chrome/browser/ui/autofill/bubble_manager.h"

namespace autofill {

class BubbleManagerImpl : public BubbleManager {
 public:
  BubbleManagerImpl();
  ~BubbleManagerImpl() override;

  BubbleManagerImpl(const BubbleManagerImpl&) = delete;
  BubbleManagerImpl& operator=(const BubbleManagerImpl&) = delete;

  // BubbleManager:
  void RequestShowController(BubbleControllerBase& controller_to_show) override;
  void OnBubbleHiddenByController(
      BubbleControllerBase& controller_to_hide) override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_BUBBLE_MANAGER_IMPL_H_
