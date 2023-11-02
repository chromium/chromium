// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LACROS_IMMERSIVE_CONTEXT_LACROS_H_
#define CHROME_BROWSER_UI_LACROS_IMMERSIVE_CONTEXT_LACROS_H_

#include "chromeos/ui/frame/immersive/immersive_context.h"

namespace chromeos {
class ImmersiveFullscreenController;
}

namespace views {
class Widget;
}

// Lacros implementation of ImmersiveContext, whose goal is to abstracts away
// the windowing related calls (eg aura, ash) for //chromeos/ui/frame/immersive.
class ImmersiveContextLacros : chromeos::ImmersiveContext {
 public:
  ImmersiveContextLacros();
  ~ImmersiveContextLacros() override;
  ImmersiveContextLacros(const ImmersiveContextLacros&) = delete;
  ImmersiveContextLacros& operator=(const ImmersiveContextLacros&) = delete;

  // chromeos::ImmersiveContext:
  void OnEnteringOrExitingImmersive(
      chromeos::ImmersiveFullscreenController* controller,
      bool entering) override;
  gfx::Rect GetDisplayBoundsInScreen(views::Widget* widget) override;
  bool DoesAnyWindowHaveCapture() override;
};

#endif  // CHROME_BROWSER_UI_LACROS_IMMERSIVE_CONTEXT_LACROS_H_
