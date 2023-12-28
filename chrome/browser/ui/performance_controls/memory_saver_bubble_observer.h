// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_OBSERVER_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_OBSERVER_H_

// This observer interface for the memory saver bubble dialog.
class MemorySaverBubbleObserver {
 public:
  // Called when the memory saver dialog is opened.
  virtual void OnBubbleShown() = 0;
  // Called when the memory saver dialog is closed.
  virtual void OnBubbleHidden() = 0;

 protected:
  virtual ~MemorySaverBubbleObserver() = default;
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_BUBBLE_OBSERVER_H_
