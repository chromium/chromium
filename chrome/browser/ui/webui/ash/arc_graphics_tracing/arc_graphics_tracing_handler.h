// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/exo/surface_observer.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"

namespace base {
class FilePath;
}  // namespace base

namespace exo {
class Surface;
class WMHelper;
}  // namespace exo

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class ArcGraphicsTracingHandler : public content::WebUIMessageHandler,
                                  public wm::ActivationChangeObserver,
                                  public aura::WindowObserver,
                                  public ui::EventHandler,
                                  public exo::SurfaceObserver {
 public:
  struct ActiveTrace;

  base::FilePath GetModelPathFromTitle(std::string_view title);

  ArcGraphicsTracingHandler();

  ArcGraphicsTracingHandler(const ArcGraphicsTracingHandler&) = delete;
  ArcGraphicsTracingHandler& operator=(const ArcGraphicsTracingHandler&) =
      delete;

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

 protected:
  // Traces stop automatically when they get this long. Visible for testing.
  base::TimeDelta max_tracing_time_ = base::Seconds(5);

 private:
  virtual aura::Window* GetWebUIWindow();
  virtual void StartTracingOnController(
      const base::trace_event::TraceConfig& trace_config,
      content::TracingController::StartTracingDoneCallback after_start);
  virtual void StopTracingOnController(
      content::TracingController::CompletionCallback after_stop);

  // For testing. This lets tests avoid casting from BrowserContext to Profile.
  virtual base::FilePath GetDownloadsFolder();

  // There is a ScopedTimeClockOverrides for tests that makes this seem
  // redundant, but it is rather awkward to have a single test base which
  // utilizes either system time or mock time, as this must be specified in
  // the constructor, and the childmost test class constructor must be
  // parameterless.
  virtual base::Time Now();

  // Exposed for testing. This implementation uses TRACE_TIME_TICKS_NOW.
  // Returns the timestamp using clock_gettime(CLOCK_MONOTONIC), which is
  // needed for comparison with trace timestamps.
  virtual base::TimeTicks SystemTicksNow();

  void ActivateWebUIWindow();
  void StartTracing();
  void StopTracing();
  void StopTracingAndActivate();
  void SetStatus(const std::string& status);

  void OnTracingStarted();
  void OnTracingStopped(std::unique_ptr<ActiveTrace> trace,
                        std::unique_ptr<std::string> trace_data);

  // Called when graphics model is built or load. Extra string parameter
  // contains a status. In case model cannot be built/load empty |base::Value|
  // is returned.
  void OnGraphicsModelReady(std::pair<base::Value, std::string> result);

  // Handlers for calls from JS.
  void HandleSetMaxTime(const base::Value::List& args);
  void HandleLoadFromText(const base::Value::List& args);

  // Updates title and icon for the active ARC window.
  void UpdateActiveArcWindowInfo();

  // Stops tracking ARC window for janks.
  void DiscardActiveArcWindow();

  std::unique_ptr<ActiveTrace> active_trace_;

  const raw_ptr<exo::WMHelper, ExperimentalAsh> wm_helper_;

  raw_ptr<aura::Window, ExperimentalAsh> arc_active_window_ = nullptr;

  base::WeakPtrFactory<ArcGraphicsTracingHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ARC_GRAPHICS_TRACING_ARC_GRAPHICS_TRACING_HANDLER_H_
