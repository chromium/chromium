// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_

#import <Cocoa/Cocoa.h>

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "ui/base/accelerators/accelerator.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

// This class maintains a map of command_ids to Accelerator objects (see
// chrome/app/chrome_command_ids.h). Currently, this only lists the commands
// that are used in the App menu.
//
// It is recommended that this class be used as a singleton so that the key map
// isn't created multiple places.
//
//   #import "base/memory/singleton.h"
//   ...
//   AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
//   return keymap->GetAcceleratorForCommand(IDC_COPY);
//
class AcceleratorsCocoa {
 public:
  typedef std::map<int, ui::Accelerator> AcceleratorMap;
  typedef std::vector<ui::Accelerator> AcceleratorVector;
  typedef AcceleratorMap::const_iterator const_iterator;

  AcceleratorsCocoa(const AcceleratorsCocoa&) = delete;
  AcceleratorsCocoa& operator=(const AcceleratorsCocoa&) = delete;

  const_iterator const begin() { return accelerators_.begin(); }
  const_iterator const end() { return accelerators_.end(); }

  // Returns NULL if there is no accelerator for the command.
  const ui::Accelerator* GetAcceleratorForCommand(int command_id);

  // Returns the singleton instance.
  static AcceleratorsCocoa* GetInstance();

  // Informs AcceleratorsCocoa that it's constructing accelerators for a PWA.
  static void CreateForPWA(bool flag);

 private:
  friend struct base::DefaultSingletonTraits<AcceleratorsCocoa>;
  FRIEND_TEST_ALL_PREFIXES(AcceleratorsCocoaBrowserTest,
                           MappingAcceleratorsInMainMenu);
  FRIEND_TEST_ALL_PREFIXES(AcceleratorsCocoaBrowserTestRTL,
                           HistoryAcceleratorsReversedForRTL);

  AcceleratorsCocoa();
  ~AcceleratorsCocoa();

  // Contains accelerators from both the app menu and the main menu.
  AcceleratorMap accelerators_;
};

#endif  // CHROME_BROWSER_UI_COCOA_ACCELERATORS_COCOA_H_
