// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
#define COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_

#include <memory>
#include <string>

#include "base/optional.h"

namespace ui {
class PropertyHandler;
}

namespace aura {
class Window;
}

namespace base {
class TimeDelta;
}

namespace ui {
class LocatedEvent;
class KeyEvent;
}

namespace exo {

class Surface;
class ShellSurfaceBase;

// Sets the application ID to the property_handler. The application ID
// identifies the general class of applications to which the window belongs.
void SetShellApplicationId(ui::PropertyHandler* property_handler,
                           const base::Optional<std::string>& id);
const std::string* GetShellApplicationId(const aura::Window* window);

// Sets the startup ID to the property handler. The startup ID identifies the
// application using startup notification protocol.
void SetShellStartupId(ui::PropertyHandler* property_handler,
                       const base::Optional<std::string>& id);
const std::string* GetShellStartupId(aura::Window* window);

// Hides/shows the shelf when fullscreen. If true, shelf is inaccessible
// (plain fullscreen). If false, shelf auto-hides and can be shown with a
// mouse gesture (immersive fullscreen).
void SetShellUseImmersiveForFullscreen(aura::Window* window, bool value);

// Sets the client accessibility ID for the window. The accessibility ID
// identifies the accessibility tree provided by client.
void SetShellClientAccessibilityId(aura::Window* window,
                                   const base::Optional<int32_t>& id);
const base::Optional<int32_t> GetShellClientAccessibilityId(
    aura::Window* window);

// Sets the root surface to the property handler.
void SetShellRootSurface(ui::PropertyHandler* property_handler,
                         Surface* surface);

// Returns the main Surface instance or nullptr if it is not set.
// |window| must not be nullptr.
Surface* GetShellRootSurface(const aura::Window* window);

// Returns the ShellSurfaceBase for the given |window|, or nullptr if no such
// surface exists.
ShellSurfaceBase* GetShellSurfaceBaseForWindow(aura::Window* window);

// Returns the target surface for the located event |event|.  If an
// event handling is grabbed by an window, it'll first examine that
// window, then traverse to its transient parent if the parent also
// requested grab.
Surface* GetTargetSurfaceForLocatedEvent(const ui::LocatedEvent* event);

// Allows the |window| to activate itself for the duration of |timeout|. Revokes
// any existing permission.
void GrantPermissionToActivate(aura::Window* window, base::TimeDelta timeout);

// Revokes the permission for |window| to activate itself.
void RevokePermissionToActivate(aura::Window* window);

// Returns true if the |window| has permission to activate itself.
bool HasPermissionToActivate(aura::Window* window);

// Returns true if event is/will be consumed by IME.
bool ConsumedByIme(aura::Window* window, const ui::KeyEvent& event);

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
