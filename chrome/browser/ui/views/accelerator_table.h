// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_
#define CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_

#include <vector>

#include "ui/events/keycodes/keyboard_codes.h"

namespace ui {
class Accelerator;
}

// This contains the list of accelerators for the Aura implementation.
struct AcceleratorMapping {
  ui::KeyboardCode keycode;
  int modifiers;
  int command_id;
};

// Returns a list of accelerator mapping information for accelerators
// handled by Chrome but excluding accelerators handled by Ash.
//
// On macOS, most accelerators are present in the main menu. The mapping from
// vkey -> command is modifiable by the user at any point in time. As such, it
// doesn't make sense to have a static mapping. Furthermore, on macOS, all
// accelerators that use a modifier key are handled much earlier during event
// processing.
//
// On macOS (only), the result will only contain accelerators that do not use a
// modifier key (e.g. escape).
std::vector<AcceleratorMapping> GetAcceleratorList();

// This function should be used only for testing. Clears the accelerator list.
void ClearAcceleratorListForTesting();

// Returns true on Ash and if the command id has an associated accelerator which
// is handled by Ash. If the return is true the accelerator is returned via the
// second argument.
bool GetAshAcceleratorForCommandId(int command_id,
                                   ui::Accelerator* accelerator);

// Returns true if the command id has an associated standard
// accelerator like cut, copy and paste. If the return is true the
// accelerator is returned via the second argument.
bool GetStandardAcceleratorForCommandId(int command_id,
                                        ui::Accelerator* accelerator);

// Returns true if the command identified by |command_id| should be executed
// repeatedly while its accelerator keys are held down.
bool IsCommandRepeatable(int command_id);

#endif  // CHROME_BROWSER_UI_VIEWS_ACCELERATOR_TABLE_H_
