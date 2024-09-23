// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_CONTEXT_H_
#define CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_CONTEXT_H_

#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

class ExclusiveAccessManager;

namespace ui {
class AcceleratorProvider;
}

// Context in which the exclusive access bubble view is initiated.
class ExclusiveAccessBubbleViewsContext {
 public:
  // Returns ExclusiveAccessManager controlling exclusive access for the given
  // webview.
  virtual ExclusiveAccessManager* GetExclusiveAccessManager() = 0;

  // Returns the AcceleratorProvider, providing the shortcut key to exit the
  // exclusive access.
  virtual ui::AcceleratorProvider* GetAcceleratorProvider() = 0;

  // Returns the view used to parent the bubble Widget.
  virtual gfx::NativeView GetBubbleParentView() const = 0;

  // Return the current bounds (not restored bounds) of the parent window.
  virtual gfx::Rect GetClientAreaBoundsInScreen() const = 0;

  // Returns true if immersive mode is enabled.
  virtual bool IsImmersiveModeEnabled() const = 0;

  // Returns the bounds of the top level View in screen coordinate system.
  virtual gfx::Rect GetTopContainerBoundsInScreen() = 0;

  // Destroy any exclusive access bubble. This allows the bubble to ask its
  // owner to clean up when the bubble observes its native widget being
  // destroyed before the owner requested it.
  virtual void DestroyAnyExclusiveAccessBubble() = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXCLUSIVE_ACCESS_BUBBLE_VIEWS_CONTEXT_H_
