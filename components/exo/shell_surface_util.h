// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
#define COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_

#include <memory>
#include <optional>
#include <string>

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
}  // namespace ui

namespace exo {

class ClientControlledShellSurface;
class Surface;
class ShellSurfaceBase;

// Sets the application ID to the property_handler. The application ID
// identifies the general class of applications to which the window belongs.
void SetShellApplicationId(ui::PropertyHandler* property_handler,
                           const std::optional<std::string>& id);
const std::string* GetShellApplicationId(const aura::Window* window);

// Sets the startup ID to the property handler. The startup ID identifies the
// application using startup notification protocol.
void SetShellStartupId(ui::PropertyHandler* property_handler,
                       const std::optional<std::string>& id);
const std::string* GetShellStartupId(const aura::Window* window);

// Shows/hides the shelf when fullscreen. If true, titlebar/shelf will show when
// the mouse moves to the top/bottom of the screen. If false (plain fullscreen),
// the titlebar and shelf are always hidden.
void SetShellUseImmersiveForFullscreen(aura::Window* window, bool value);

// Sets the client accessibility ID for the window. The accessibility ID
// identifies the accessibility tree provided by client.
void SetShellClientAccessibilityId(aura::Window* window,
                                   const std::optional<int32_t>& id);
const std::optional<int32_t> GetShellClientAccessibilityId(
    aura::Window* window);

// Sets the ClientControlledShellSurface to the property handler.
void SetShellClientControlledShellSurface(
    ui::PropertyHandler* property_handler,
    const std::optional<ClientControlledShellSurface*>& shell_surface);
ClientControlledShellSurface* GetShellClientControlledShellSurface(
    ui::PropertyHandler* property_handler);

// Returns |index| for the window.
// Returns -1 for |index| when window is visible on all workspaces,
// otherwise, 0-based indexing for desk index.
int GetWindowDeskStateChanged(const aura::Window* window);

// Sets the root surface to the property handler.
void SetShellRootSurface(ui::PropertyHandler* property_handler,
                         Surface* surface);

// Returns the main Surface instance or nullptr if it is not set.
// |window| must not be nullptr.
Surface* GetShellRootSurface(const aura::Window* window);

// Returns the ShellSurfaceBase for the given |window|, or nullptr if no such
// surface exists.
ShellSurfaceBase* GetShellSurfaceBaseForWindow(const aura::Window* window);

// Returns the target surface for the located event |event|.  If an
// event handling is grabbed by an window, it'll first examine that
// window, then traverse to its transient parent if the parent also
// requested grab.
Surface* GetTargetSurfaceForLocatedEvent(const ui::LocatedEvent* event);

// Returns the focused surface for given 'focused_window'.  If a surface is
// attached to the window, this will return that surface.  If the window is
// either the shell surface's window, or host window, it will return the root
// surface, otherwise returns nullptr.
Surface* GetTargetSurfaceForKeyboardFocus(aura::Window* focused_window);

// Allows the |window| to activate itself for the duration of |timeout|. Revokes
// any existing permission.
void GrantPermissionToActivate(aura::Window* window, base::TimeDelta timeout);

// Allows the |window| to activate itself indefinitely. Revokes any existing
// permission.
void GrantPermissionToActivateIndefinitely(aura::Window* window);

// Revokes the permission for |window| to activate itself.
void RevokePermissionToActivate(aura::Window* window);

// Returns true if the |window| has permission to activate itself.
bool HasPermissionToActivate(aura::Window* window);

// Returns true if event is/will be consumed by IME.
bool ConsumedByIme(const ui::KeyEvent& event);

// Set aura::client::kSkipImeProcessing to all Surface descendants.
void SetSkipImeProcessingToDescendentSurfaces(aura::Window* window, bool value);

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_UTIL_H_
