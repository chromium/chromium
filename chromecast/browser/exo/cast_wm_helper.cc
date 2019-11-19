// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/exo/cast_wm_helper.h"

#include "base/memory/singleton.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/wm/public/activation_client.h"

namespace exo {
namespace {

// Returns the native location of the display. Removes any rotations and scales.
gfx::Rect GetNativeBounds(const display::Display& display) {
  gfx::Point origin = gfx::ScaleToFlooredPoint(display.bounds().origin(),
                                               display.device_scale_factor());
  gfx::Size size_in_pixels = display.GetSizeInPixel();
  switch (display.rotation()) {
    case display::Display::ROTATE_0:
    case display::Display::ROTATE_180:
      return gfx::Rect(origin, size_in_pixels);
    case display::Display::ROTATE_90:
    case display::Display::ROTATE_270:
      return gfx::Rect(
          origin, gfx::Size(size_in_pixels.height(), size_in_pixels.width()));
  }
}

std::vector<display::ManagedDisplayMode> GetDisplayModes(
    const display::Display& display) {
  display::ManagedDisplayMode mode(GetNativeBounds(display).size(), 60.f, false,
                                   true, 1.f);
  return std::vector<display::ManagedDisplayMode>(1, mode);
}

}  // namespace

CastWMHelper::CastWMHelper(
    chromecast::CastWindowManagerAura* cast_window_manager_aura,
    chromecast::CastScreen* cast_screen)
    : cast_window_manager_aura_(cast_window_manager_aura),
      cast_screen_(cast_screen),
      vsync_timing_manager_(this) {
  cast_screen_->AddObserver(&display_observer_);
  for (const auto& display : cast_screen_->GetAllDisplays())
    display_observer_.OnDisplayAdded(display);
}

CastWMHelper::~CastWMHelper() {
  cast_screen_->RemoveObserver(&display_observer_);
}

void CastWMHelper::AddActivationObserver(
    wm::ActivationChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::AddFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::AddDragDropObserver(DragDropObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::RemoveDragDropObserver(DragDropObserver* observer) {
  NOTIMPLEMENTED();
}

void CastWMHelper::SetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, this);
}

void CastWMHelper::ResetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, nullptr);
}

VSyncTimingManager& CastWMHelper::GetVSyncTimingManager() {
  return vsync_timing_manager_;
}

void CastWMHelper::OnDragEntered(const ui::DropTargetEvent& event) {}

int CastWMHelper::OnDragUpdated(const ui::DropTargetEvent& event) {
  NOTIMPLEMENTED();
  return 0;
}

void CastWMHelper::OnDragExited() {}

int CastWMHelper::OnPerformDrop(const ui::DropTargetEvent& event,
                                std::unique_ptr<ui::OSExchangeData> data) {
  NOTIMPLEMENTED();
  return ui::DragDropTypes::DRAG_MOVE;
}

void CastWMHelper::AddVSyncParameterObserver(
    mojo::PendingRemote<viz::mojom::VSyncParameterObserver> observer) {
  cast_window_manager_aura_->GetRootWindow()
      ->layer()
      ->GetCompositor()
      ->AddVSyncParameterObserver(std::move(observer));
}

const display::ManagedDisplayInfo& CastWMHelper::GetDisplayInfo(
    int64_t display_id) const {
  return display_observer_.GetDisplayInfo(display_id);
}

const std::vector<uint8_t>& CastWMHelper::GetDisplayIdentificationData(
    int64_t display_id) const {
  NOTIMPLEMENTED();
  static std::vector<uint8_t> no_data;
  return no_data;
}

bool CastWMHelper::GetActiveModeForDisplayId(
    int64_t display_id,
    display::ManagedDisplayMode* mode) const {
  return display_observer_.GetActiveModeForDisplayId(display_id, mode);
}

aura::Window* CastWMHelper::GetPrimaryDisplayContainer(int container_id) {
  return cast_window_manager_aura_->GetRootWindow();
}

aura::Window* CastWMHelper::GetActiveWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* CastWMHelper::GetFocusedWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* CastWMHelper::GetRootWindowForNewWindows() const {
  return cast_window_manager_aura_->GetRootWindow();
}

aura::client::CursorClient* CastWMHelper::GetCursorClient() {
  NOTIMPLEMENTED();
  return nullptr;
}

void CastWMHelper::AddPreTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->AddPreTargetHandler(handler);
}

void CastWMHelper::PrependPreTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void CastWMHelper::RemovePreTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->RemovePreTargetHandler(handler);
}

void CastWMHelper::AddPostTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->AddPostTargetHandler(handler);
}

void CastWMHelper::RemovePostTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->RemovePostTargetHandler(handler);
}

bool CastWMHelper::InTabletMode() const {
  NOTIMPLEMENTED();
  return false;
}

double CastWMHelper::GetDefaultDeviceScaleFactor() const {
  NOTIMPLEMENTED();
  return 1.0;
}

void CastWMHelper::SetImeBlocked(aura::Window* window, bool ime_blocked) {
  NOTIMPLEMENTED();
}

bool CastWMHelper::IsImeBlocked(aura::Window* window) const {
  NOTIMPLEMENTED();
  return false;
}

WMHelper::LifetimeManager* CastWMHelper::GetLifetimeManager() {
  return &lifetime_manager_;
}

aura::client::CaptureClient* CastWMHelper::GetCaptureClient() {
  return cast_window_manager_aura_->capture_client();
}

CastWMHelper::CastDisplayObserver::CastDisplayObserver() {}

CastWMHelper::CastDisplayObserver::~CastDisplayObserver() {}

void CastWMHelper::CastDisplayObserver::OnWillProcessDisplayChanges() {}

void CastWMHelper::CastDisplayObserver::OnDidProcessDisplayChanges() {}

void CastWMHelper::CastDisplayObserver::OnDisplayAdded(
    const display::Display& new_display) {
  display::ManagedDisplayInfo md(new_display.id(), "CastDisplayInfo", true);
  md.SetRotation(new_display.rotation(),
                 display::Display::RotationSource::ACTIVE);
  md.SetBounds(GetNativeBounds(new_display));
  md.SetManagedDisplayModes(GetDisplayModes(new_display));
  md.set_native(true);
  display_info_.emplace(new_display.id(), md);
}

void CastWMHelper::CastDisplayObserver::OnDisplayRemoved(
    const display::Display& old_display) {
  display_info_.erase(old_display.id());
}

void CastWMHelper::CastDisplayObserver::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display_info_.find(display.id()) == display_info_.end())
    OnDisplayAdded(display);

  // Currently only updates bounds
  if ((DISPLAY_METRIC_BOUNDS & changed_metrics) == DISPLAY_METRIC_BOUNDS)
    display_info_[display.id()].SetBounds(GetNativeBounds(display));
}

const display::ManagedDisplayInfo&
CastWMHelper::CastDisplayObserver::GetDisplayInfo(int64_t display_id) const {
  auto iter = display_info_.find(display_id);
  DCHECK(iter != display_info_.end())
      << "Failed to find display " << display_id;
  return iter->second;
}

bool CastWMHelper::CastDisplayObserver::GetActiveModeForDisplayId(
    int64_t display_id,
    display::ManagedDisplayMode* mode) const {
  auto iter = display_info_.find(display_id);
  DCHECK(iter != display_info_.end())
      << "Failed to find display " << display_id;
  for (const auto& display_mode : iter->second.display_modes()) {
    if (display_mode.native()) {
      *mode = display_mode;
      return true;
    }
  }

  return false;
}

}  // namespace exo
