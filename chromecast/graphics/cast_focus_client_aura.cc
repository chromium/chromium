// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/graphics/cast_focus_client_aura.h"

#include "base/stl_util.h"
#include "ui/aura/window.h"

#define LOG_WINDOW_INFO(top_level, window)                              \
  "top-level: " << (top_level)->id() << ": '" << (top_level)->GetName() \
                << "', window: " << (window)->id() << ": '"             \
                << (window)->GetName() << "'"

namespace chromecast {

CastFocusClientAura::CastFocusClientAura() : focused_window_(nullptr) {}

CastFocusClientAura::~CastFocusClientAura() {
  focused_window_ = nullptr;
  for (aura::Window* window : focusable_windows_) {
    window->RemoveObserver(this);
  }
  focusable_windows_.clear();
}

aura::Window* CastFocusClientAura::GetFocusedWindow() {
  return focused_window_;
}

void CastFocusClientAura::AddObserver(
    aura::client::FocusChangeObserver* observer) {
  focus_observers_.AddObserver(observer);
}

void CastFocusClientAura::RemoveObserver(
    aura::client::FocusChangeObserver* observer) {
  focus_observers_.RemoveObserver(observer);
}

void CastFocusClientAura::OnWindowVisibilityChanged(aura::Window* window,
                                                    bool visible) {
  if (!visible && (window == focused_window_)) {
    // The focused window just lost visibility, so de-focus it.
    UpdateWindowFocus();
  } else if (visible) {
    // The window that just became visible might be the most appropriate window
    // to have focus.
    UpdateWindowFocus();
  }
}

// One of our observed windows is being destroyed.
// We observe each window that has the potential for being focused,
// so this window needs to be removed from the list of focusable windows.
void CastFocusClientAura::OnWindowDestroying(aura::Window* window) {
  aura::Window* top_level = GetZOrderWindow(window);
  DCHECK(top_level);
  DLOG(INFO) << "Removing window, " << LOG_WINDOW_INFO(top_level, window);

  auto iter =
      std::find(focusable_windows_.begin(), focusable_windows_.end(), window);
  if (iter != focusable_windows_.end()) {
    focusable_windows_.erase(iter);
    window->RemoveObserver(this);
  }
  if (window == focused_window_) {
    // De-focus the window that is being destroyed.
    UpdateWindowFocus();
  }
}

// Update focus if a window is entering or leaving our hierarchy.
void CastFocusClientAura::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  if (params.new_parent &&
      (aura::client::GetFocusClient(params.new_parent) == this)) {
    if (params.old_parent == params.new_parent) {
      // A window is moving within our hierarchy.
      return;
    } else {
      // A window is entering our hierarchy, so we need to consider
      // focusing it.
      FocusWindow(params.target);
      return;
    }
  }

  // The window is leaving our hierarchy, so stop tracking it.
  // It could contain multiple windows that were focused, so lets stop tracking
  // them all.
  auto iter = focusable_windows_.begin();
  bool was_focused = false;
  while (iter != focusable_windows_.end()) {
    aura::Window* window = *iter;
    if (params.target == window || params.target->Contains(window)) {
      window->RemoveObserver(this);
      was_focused |= window == focused_window_;
      iter = focusable_windows_.erase(iter);

      aura::Window* top_level = GetZOrderWindow(window);
      DCHECK(top_level);
      DLOG(INFO) << "Dropping window, " << LOG_WINDOW_INFO(top_level, window);
    } else {
      ++iter;
    }
  }

  if (was_focused) {
    // The window that was removed from our hierarchy was the focused window, so
    // de-focus it.
    UpdateWindowFocus();
  }
}

// An explicit request to focus a window.
// We lock focus to the top-most high-level window, and so will ignore this
// focus request if it isn't for the topmost window.  If it is for a lower
// window, then we'll track it to focus it later when it rises to the top.
void CastFocusClientAura::FocusWindow(aura::Window* window) {
  if (window) {
    if (!window->CanFocus()) {
      return;
    }
    aura::Window* top_level = GetZOrderWindow(window);
    DCHECK(top_level);
    DLOG(INFO) << "Requesting focus for " << LOG_WINDOW_INFO(top_level, window);
    if (!base::Contains(focusable_windows_, window)) {
      // We're not yet tracking this focusable window, so start tracking it as a
      // potential focus target.
      window->AddObserver(this);
      focusable_windows_.push_back(window);
    }
  }

  // Check whether this new window is the most appropriate to focus.
  UpdateWindowFocus();
}

// Finds the top-most window, and if it doesn't have focus, then gives it focus.
void CastFocusClientAura::UpdateWindowFocus() {
  aura::Window* window = GetWindowToFocus();
  if (window == focused_window_) {
    return;
  }

  if (window) {
    aura::Window* top_level = GetZOrderWindow(window);
    DCHECK(top_level);
    DLOG(INFO) << "Switching focus to " << LOG_WINDOW_INFO(top_level, window);
  }

  aura::Window* unfocus_window = focused_window_;
  focused_window_ = window;

  for (aura::client::FocusChangeObserver& observer : focus_observers_) {
    observer.OnWindowFocused(focused_window_, unfocus_window);
    if (focused_window_ != window) {
      // The observer changed focused_window_.
      return;
    }
  }

  if (unfocus_window) {
    aura::client::FocusChangeObserver* focus_observer =
        aura::client::GetFocusChangeObserver(unfocus_window);
    if (focus_observer) {
      focus_observer->OnWindowFocused(focused_window_, unfocus_window);
      if (focused_window_ != window) {
        // The observer changed focused_window_.
        return;
      }
    }
  }
  if (focused_window_) {
    aura::client::FocusChangeObserver* focus_observer =
        aura::client::GetFocusChangeObserver(focused_window_);
    if (focus_observer) {
      focus_observer->OnWindowFocused(focused_window_, unfocus_window);
      if (focused_window_ != window) {
        // The observer changed focused_window_.
        return;
      }
    }
  }
}

// Returns the most appropriate window to have focus.
// A focusable window could be anywhere within its window hierarchy, and we
// choose based on the z-order of the top-level window in its hierarchy.
aura::Window* CastFocusClientAura::GetWindowToFocus() {
  aura::Window* next = nullptr;
  aura::Window* next_top_level = nullptr;
  for (aura::Window* window : focusable_windows_) {
    if (!window->CanFocus() || !window->IsVisible()) {
      continue;
    }

    // Compare z-order of top-level windows using the window IDs.
    aura::Window* top_level = GetZOrderWindow(window);
    DCHECK(top_level);
    if (!next || top_level->id() >= next_top_level->id()) {
      next = window;
      next_top_level = top_level;
    }
  }
  return next;
}

const aura::Window* CastFocusClientAura::GetZOrderWindow(
    const aura::Window* window) const {
  while (window->parent() && !window->parent()->IsRootWindow()) {
    window = window->parent();
  }
  return window;
}

void CastFocusClientAura::ResetFocusWithinActiveWindow(aura::Window* window) {
  // Sets focus to |window| if it's within the active window (a child of the
  // focused window).
  if (focused_window_ && focused_window_->Contains(window)) {
    FocusWindow(window);
  }
}

void CastFocusClientAura::AddObserver(wm::ActivationChangeObserver* observer) {}

void CastFocusClientAura::RemoveObserver(
    wm::ActivationChangeObserver* observer) {}

void CastFocusClientAura::ActivateWindow(aura::Window* window) {}

void CastFocusClientAura::DeactivateWindow(aura::Window* window) {}

const aura::Window* CastFocusClientAura::GetActiveWindow() const {
  return nullptr;
}

aura::Window* CastFocusClientAura::GetActivatableWindow(
    aura::Window* window) const {
  return window;
}

const aura::Window* CastFocusClientAura::GetToplevelWindow(
    const aura::Window* window) const {
  return GetZOrderWindow(window);
}

bool CastFocusClientAura::CanActivateWindow(const aura::Window* window) const {
  return true;
}

}  // namespace chromecast
