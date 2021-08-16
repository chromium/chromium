// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
#define COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "components/arc/compat_mode/resize_toggle_menu.h"

namespace views {
class Widget;
}  // namespace views

namespace arc {

class ArcResizeLockPrefDelegate;

class CompatModeButtonController {
 public:
  CompatModeButtonController();
  CompatModeButtonController(const CompatModeButtonController&) = delete;
  CompatModeButtonController& operator=(const CompatModeButtonController&) =
      delete;
  virtual ~CompatModeButtonController();

  // virtual for unittest.
  virtual void Update(ArcResizeLockPrefDelegate* pref_delegate,
                      aura::Window* window);

  base::WeakPtr<CompatModeButtonController> GetWeakPtr();

 private:
  void ToggleResizeToggleMenu(views::Widget* widget,
                              ArcResizeLockPrefDelegate* pref_delegate);

  std::unique_ptr<ResizeToggleMenu> resize_toggle_menu_;

  base::WeakPtrFactory<CompatModeButtonController> weak_ptr_factory_{this};
};

}  // namespace arc

#endif  // COMPONENTS_ARC_COMPAT_MODE_COMPAT_MODE_BUTTON_CONTROLLER_H_
