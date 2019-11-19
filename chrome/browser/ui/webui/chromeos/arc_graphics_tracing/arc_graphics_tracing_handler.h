// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/webui/chromeos/arc_graphics_tracing/arc_graphics_tracing.h"
#include "components/exo/surface_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace arc {
class ArcGraphicsJankDetector;
class ArcSystemStatCollector;
}  // namespace arc

namespace base {
class ListValue;
}  // namespace base

namespace exo {
class Surface;
class WMHelper;
}  // namespace exo

namespace chromeos {

class ArcGraphicsTracingHandler : public content::WebUIMessageHandler,
                                  public wm::ActivationChangeObserver,
                                  public aura::WindowObserver,
                                  public ui::EventHandler,
                                  public exo::SurfaceObserver {
 public:
  explicit ArcGraphicsTracingHandler(ArcGraphicsTracingMode mode);
  ~ArcGraphicsTracingHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;

  // exo::SurfaceObserver:
  void OnSurfaceDestroying(exo::Surface* surface) override;
  void OnCommit(exo::Surface* surface) override;

 private:
  void Activate();
  void StartTracing();
  void StopTracing();
  void StopTracingAndActivate();
  void SetStatus(const std::string& status);

  void OnTracingStarted();
  void OnTracingStopped(std::unique_ptr<std::string> trace_data);

  // Called when graphics model is built or load. Extra string parameter
  // contains a status. In case model cannot be built/load empty |base::Value|
  // is returned.
  void OnGraphicsModelReady(std::pair<base::Value, std::string> result);

  // Handlers for calls from JS.
  void HandleReady(const base::ListValue* args);
  void HandleSetStopOnJank(const base::ListValue* args);
  void HandleSetMaxTime(const base::ListValue* args);
  void HandleLoadFromText(const base::ListValue* args);

  // Updates title and icon for the active ARC window.
  void UpdateActiveArcWindowInfo();

  // Stops tracking ARC window for janks.
  void DiscardActiveArcWindow();

  // Called in case jank is detected in active ARC window.
  void OnJankDetected(const base::Time& timestamp);

  // Returns max sampling interval to display.
  base::TimeDelta GetMaxInterval() const;

  // Indicates that tracing was initiated by this handler.
  bool tracing_active_ = false;

  // Determines if tracing should stop in case jank is detected runtime.
  // Works only in |ArcGraphicsTracingMode::kFull| mode.
  bool stop_on_jank_ = true;

  // Determines the maximum tracing time.
  // Works only in |ArcGraphicsTracingMode::kOverview| mode.
  base::TimeDelta max_tracing_time_ = base::TimeDelta::FromSeconds(5);

  base::OneShotTimer stop_tracing_timer_;

  exo::WMHelper* const wm_helper_;

  const ArcGraphicsTracingMode mode_;

  aura::Window* arc_active_window_ = nullptr;

  // Time filter for tracing, since ARC++ window was activated last until
  // tracing is stopped.
  base::TimeTicks tracing_time_min_;
  base::TimeTicks tracing_time_max_;

  // Task id and title for current ARC window.
  int active_task_id_ = -1;

  // Used to detect janks for the currently active ARC++ window.
  std::unique_ptr<arc::ArcGraphicsJankDetector> jank_detector_;

  // Collects system stat runtime.
  std::unique_ptr<arc::ArcSystemStatCollector> system_stat_colletor_;

  // Information about active task, title and icon.
  std::string active_task_title_;
  std::vector<unsigned char> active_task_icon_png_;
  base::Time timestamp_;

  base::WeakPtrFactory<ArcGraphicsTracingHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcGraphicsTracingHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
