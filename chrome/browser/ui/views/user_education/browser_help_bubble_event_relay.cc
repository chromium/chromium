// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/user_education/browser_help_bubble_event_relay.h"

#include "ui/gfx/native_widget_types.h"

#if USE_AURA
#include "ui/aura/window.h"
#endif

WindowHelpBubbleEventRelay::WindowHelpBubbleEventRelay(
    views::Widget* source_widget)
    : source_widget_(source_widget) {
  widget_observation_.Observe(source_widget_);
}

WindowHelpBubbleEventRelay::~WindowHelpBubbleEventRelay() {
  CHECK(!source_widget_) << "Should have been released in derived class.";
}

bool WindowHelpBubbleEventRelay::ShouldHelpBubbleProcessEvents() const {
  return false;
}

bool WindowHelpBubbleEventRelay::ShouldUnHoverOnMouseExit() const {
  return true;
}

void WindowHelpBubbleEventRelay::Release() {
  if (source_widget_) {
    widget_observation_.Reset();
    source_widget_ = nullptr;
  }
}

void WindowHelpBubbleEventRelay::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(source_widget_.get(), widget);
  Release();
}

std::unique_ptr<WindowHelpBubbleEventRelay> CreateWindowHelpBubbleEventRelay(
    views::Widget* source_widget) {
#if USE_AURA
  return std::make_unique<WindowHelpBubbleEventRelayAura>(source_widget);
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<WindowHelpBubbleEventRelayMac>(source_widget);
#else
  NOTREACHED();
#endif
}

#if USE_AURA

WindowHelpBubbleEventRelayAura::WindowHelpBubbleEventRelayAura(
    views::Widget* source_widget)
    : WindowHelpBubbleEventRelay(source_widget) {
  WindowHelpBubbleEventRelay::source_widget()
      ->GetNativeWindow()
      ->AddPreTargetHandler(this);
}

WindowHelpBubbleEventRelayAura::~WindowHelpBubbleEventRelayAura() {
  Release();
}

void WindowHelpBubbleEventRelayAura::OnEvent(ui::Event* event) {
  if (event->IsLocatedEvent()) {
    auto* const located = event->AsLocatedEvent();
    // When dealing with events sent directly to an aura::Window, the root
    // location is not yet computed.
    const gfx::Point screen_coords =
        located->target()->GetScreenLocation(*located);
    if (HelpBubbleEventRelay::OnEvent(*located, screen_coords)) {
      event->SetHandled();
    }
  }
}

void WindowHelpBubbleEventRelayAura::Release() {
  if (source_widget()) {
    source_widget()->GetNativeWindow()->RemovePreTargetHandler(this);
  }
  WindowHelpBubbleEventRelay::Release();
}

#endif  // USE_AURA
