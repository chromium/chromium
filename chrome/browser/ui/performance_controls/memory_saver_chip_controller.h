// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_CONTROLLER_H_

#include "base/byte_size.h"
#include "base/cancelable_callback.h"
#include "base/memory/raw_ref.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace page_actions {
class PageActionController;
}

namespace tabs {
class TabInterface;
}

namespace memory_saver {

// This controller provides the link between the TabHelper-based MemorySaver
// logic (which observes the WebContents and computes desired location bar
// chip state) and the Page Action framework.
class MemorySaverChipController {
 public:
  DECLARE_USER_DATA(MemorySaverChipController);

  MemorySaverChipController(
      tabs::TabInterface& tab,
      page_actions::PageActionController& page_action_controller);
  ~MemorySaverChipController();

  static MemorySaverChipController* From(tabs::TabInterface* tab);

  // These methods set the MemorySaver page action icon (or chip) to the
  // desired state. Note that chip state is transient.
  void ShowIcon();
  void ShowEducationChip();
  void ShowMemorySavedChip(base::ByteSize bytes_saved);
  void Hide();

 private:
  void StartChipTimer();
  void CancelChipTimer();
  void OnChipTimeout();

  base::WeakPtr<MemorySaverChipController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  raw_ref<page_actions::PageActionController> page_action_controller_;
  base::CancelableOnceClosure chip_timer_callback_;
  ui::ScopedUnownedUserData<MemorySaverChipController>
      scoped_unowned_user_data_;
  base::WeakPtrFactory<MemorySaverChipController> weak_factory_{this};
};

}  // namespace memory_saver

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_CHIP_CONTROLLER_H_
