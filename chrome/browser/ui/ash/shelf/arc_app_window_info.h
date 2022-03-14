// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_
#define CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_

#include <string>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "chrome/browser/ui/ash/shelf/arc_app_shelf_id.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image_skia.h"

// The information about the ARC application window which has to be kept
// even when its AppWindow is not present.
class ArcAppWindowInfo {
 public:
  ArcAppWindowInfo(const arc::ArcAppShelfId& app_shelf_id,
                   const std::string& launch_intent,
                   const std::string& package_name);
  ~ArcAppWindowInfo();

  ArcAppWindowInfo(const ArcAppWindowInfo&) = delete;
  ArcAppWindowInfo& operator=(const ArcAppWindowInfo&) = delete;

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

  aura::Window* window_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_ARC_APP_WINDOW_INFO_H_
