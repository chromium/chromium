// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shelf/arc_app_window_info.h"

#include "ash/components/arc/app/arc_app_constants.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ash/app_list/arc/intent.h"

namespace {

// Prefix in intent that specifies a logical window. Among a group of windows
// belonging to the same logical window, only one will be represented in the
// shelf and in the alt-tab menu. S. means string type.
constexpr char kLogicalWindowIntentPrefix[] =
    "S.org.chromium.arc.logical_window_id=";

std::string GetLogicalWindowIdFromIntent(const std::string& launch_intent) {
  auto intent = arc::Intent::Get(launch_intent);
  if (!intent)
    return std::string();
  const std::string prefix(kLogicalWindowIntentPrefix);
  for (const auto& param : intent->extra_params()) {
    if (base::StartsWith(param, prefix, base::CompareCase::SENSITIVE))
      return param.substr(prefix.length());
  }
  return std::string();
}

}  // namespace

ArcAppWindowInfo::ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                                   const std::string& launch_intent,
                                   const std::string& package_name)
    : app_shelf_id_(app_shelf_id),
      launch_intent_(launch_intent),
      package_name_(package_name),
      logical_window_id_(GetLogicalWindowIdFromIntent(launch_intent)) {}

ArcAppWindowInfo::~ArcAppWindowInfo() = default;

void ArcAppWindowInfo::OnWindowDestroying(aura::Window* window) {
  DCHECK(observed_window_.IsObservingSource(window));
  observed_window_.Reset();
  window_ = nullptr;
}

void ArcAppWindowInfo::SetDescription(const std::string& title,
                                      const gfx::ImageSkia& icon) {
  DCHECK(base::IsStringUTF8(title));
  title_ = title;

  // Chrome has custom Play Store icon. Don't overwrite it.
  if (app_shelf_id_.app_id() == arc::kPlayStoreAppId)
    return;
  icon_ = icon;
}

void ArcAppWindowInfo::set_window_hidden_from_shelf(bool hidden) {
  if (window_hidden_from_shelf_ != hidden) {
    window_hidden_from_shelf_ = hidden;
    UpdateWindowProperties();
  }
}

void ArcAppWindowInfo::UpdateWindowProperties() {
  aura::Window* const win = window();
  if (!win)
    return;
  bool hidden = window_hidden_from_shelf_ || task_hidden_from_shelf_;
  win->SetProperty(ash::kHideInDeskMiniViewKey, hidden);
  win->SetProperty(ash::kHideInOverviewKey, hidden);
  win->SetProperty(ash::kHideInShelfKey, hidden);
}

void ArcAppWindowInfo::set_window(aura::Window* window) {
  if (window_ == window)
    return;

  if (window_ && observed_window_.IsObservingSource(window_.get())) {
    observed_window_.Reset();
  }

  window_ = window;
  UpdateWindowProperties();

  if (window && !observed_window_.IsObservingSource(window))
    observed_window_.Observe(window);
}

aura::Window* ArcAppWindowInfo::ArcAppWindowInfo::window() {
  return window_;
}

const arc::ArcAppShelfId& ArcAppWindowInfo::app_shelf_id() const {
  return app_shelf_id_;
}

ash::ShelfID ArcAppWindowInfo::shelf_id() const {
  return ash::ShelfID(app_shelf_id_.ToString());
}

const std::string& ArcAppWindowInfo::launch_intent() const {
  return launch_intent_;
}

const std::string& ArcAppWindowInfo::package_name() const {
  return package_name_;
}

const std::string& ArcAppWindowInfo::title() const {
  return title_;
}

const gfx::ImageSkia& ArcAppWindowInfo::icon() const {
  return icon_;
}

const std::string& ArcAppWindowInfo::logical_window_id() const {
  return logical_window_id_;
}

void ArcAppWindowInfo::set_task_hidden_from_shelf() {
  if (!task_hidden_from_shelf_) {
    task_hidden_from_shelf_ = true;
    UpdateWindowProperties();
  }
}

bool ArcAppWindowInfo::task_hidden_from_shelf() const {
  return task_hidden_from_shelf_;
}
