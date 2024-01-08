// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/image/image_skia.h"

// The information about the ARC application window which has to be kept
// even when its AppWindow is not present.
class ArcAppWindowInfo : public aura::WindowObserver {
 public:
  ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                   const std::string& launch_intent,
                   const std::string& package_name);
  ~ArcAppWindowInfo() override;

  ArcAppWindowInfo(const ArcAppWindowInfo&) = delete;
  ArcAppWindowInfo& operator=(const ArcAppWindowInfo&) = delete;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  void SetDescription(const std::string& title, const gfx::ImageSkia& icon);

  void set_window(aura::Window* window);

  void set_window_hidden_from_shelf(bool hidden);

  void set_task_hidden_from_shelf();

  aura::Window* window();

  const arc::ArcAppShelfId& app_shelf_id() const;

  ash::ShelfID shelf_id() const;

  const std::string& launch_intent() const;

  const std::string& package_name() const;

  const std::string& title() const;

  const gfx::ImageSkia& icon() const;

  const std::string& logical_window_id() const;

  bool task_hidden_from_shelf() const;

 private:
  // Updates window properties depending on the window_hidden_from_shelf_ and
  // task_hidden_from_shelf_ settings.
  void UpdateWindowProperties();

  const arc::ArcAppShelfId app_shelf_id_;
  const std::string launch_intent_;
  const std::string package_name_;
  const std::string logical_window_id_;
  bool window_hidden_from_shelf_ = false;
  bool task_hidden_from_shelf_ = false;
  // Keeps overridden window title.
  std::string title_;
  // Keeps overridden window icon.
  gfx::ImageSkia icon_;

  raw_ptr<aura::Window> window_ = nullptr;

  // Scoped list of observed windows (for removal on destruction)
  // TODO(crbug.com/1323913, crbug.com/1316374): When the window is destroyed,
  // it looks like that set_window is not set in AppServiceAppWindowArcTracker
  // for an unknown reason. So observe windows in ArcAppWindowInfo, to clear
  // window_, when the window is destroyed. When the root cause is figured out,
  // observed_windows_ can be removed.
  base::ScopedObservation<aura::Window, aura::WindowObserver> observed_window_{
      this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_
