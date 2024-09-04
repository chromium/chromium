// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_list_observer.h"

class Browser;

///////////////////////////////////////////////////////////////////////////////
// ReadAnythingCoordinator
//
//  A class that coordinates the Read Anything feature. This class registers
//  itself as a SidePanelEntry.
//  The coordinator acts as the external-facing API for the Read Anything
//  feature. Classes outside this feature should make calls to the coordinator.
//  This class has the same lifetime as the browser.
//
class ReadAnythingCoordinator : public BrowserListObserver {
 public:
  explicit ReadAnythingCoordinator(Browser* browser);
  ~ReadAnythingCoordinator() override;

  // This class does not do anything until Initialize is called.
  void Initialize();

 private:
  friend class ReadAnythingCoordinatorTest;
  friend class ReadAnythingCoordinatorScreen2xDataCollectionModeTest;

  // Owns this.
  raw_ptr<Browser> browser_;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  base::WeakPtrFactory<ReadAnythingCoordinator> weak_ptr_factory_{this};
};
#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_READ_ANYTHING_READ_ANYTHING_COORDINATOR_H_
