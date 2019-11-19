// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/vr/base_graphics_delegate.h"
#include "chrome/browser/vr/testapp/gl_renderer.h"
#include "chrome/browser/vr/testapp/vr_test_context.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/ozone/public/cursor_factory_ozone.h"
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window_base.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/platform_window_init_properties.h"

// This file is a shameless rip-off of ui/ozone's demo application. Ozone lets
// us spin up a window and collect input without dealing with Linux platform
// specifics.

class AppWindow;

class RendererFactory {
 public:
  RendererFactory();
  ~RendererFactory();

  bool Initialize();
  std::unique_ptr<vr::GlRenderer> CreateRenderer(gfx::AcceleratedWidget widget);

 private:
  // Helper for applications that do GL on main thread.
  ui::OzoneGpuTestHelper gpu_helper_;

  DISALLOW_COPY_AND_ASSIGN(RendererFactory);
};

class WindowManager : public display::NativeDisplayObserver {
 public:
  explicit WindowManager(const base::Closure& quit_closure);
  ~WindowManager() override;

  void Quit();

  void AddWindow(AppWindow* window);
  void RemoveWindow(AppWindow* window);

 private:
  void OnDisplaysAquired(
      const std::vector<display::DisplaySnapshot*>& displays);
  void OnDisplayConfigured(const gfx::Rect& bounds, bool success);

  // display::NativeDisplayDelegate:
  void OnConfigurationChanged() override;
  void OnDisplaySnapshotsInvalidated() override;

  std::unique_ptr<display::NativeDisplayDelegate> delegate_;
  base::Closure quit_closure_;
  RendererFactory renderer_factory_;
  std::vector<std::unique_ptr<AppWindow>> windows_;

  // Flags used to keep track of the current state of display configuration.
  //
  // True if configuring the displays. In this case a new display configuration
  // isn't started.
  bool is_configuring_ = false;

  // If |is_configuring_| is true and another display configuration event
  // happens, the event is deferred. This is set to true and a display
  // configuration will be scheduled after the current one finishes.
  bool should_configure_ = false;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

class AppWindow : public ui::PlatformWindowDelegate {
 public:
  AppWindow(WindowManager* window_manager,
            RendererFactory* renderer_factory,
            const gfx::Rect& bounds)
      : window_manager_(window_manager),
        renderer_factory_(renderer_factory),
        weak_ptr_factory_(this) {
    ui::PlatformWindowInitProperties properties;
    properties.bounds = gfx::Rect(1024, 768);
    platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
        this, std::move(properties));
    platform_window_->Show();

    // Supply an empty cursor to override and hide the default system pointer.
    platform_window_->SetCursor(
        ui::CursorFactoryOzone::GetInstance()->CreateImageCursor(SkBitmap(),
                                                                 {0, 0}, 0));
  }
  ~AppWindow() override {}

  gfx::AcceleratedWidget GetAcceleratedWidget() {
    DCHECK_NE(widget_, gfx::kNullAcceleratedWidget)
        << "Widget not available synchronously";
    return widget_;
  }

  void Start() {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppWindow::StartOnGpu, weak_ptr_factory_.GetWeakPtr()));
  }

  void Quit() { window_manager_->Quit(); }

  // PlatformWindowDelegate:
  void OnBoundsChanged(const gfx::Rect& new_bounds) override {
    vr_context_->set_window_size(new_bounds.size());
  }
  void OnDamageRect(const gfx::Rect& damaged_region) override {}
  void DispatchEvent(ui::Event* event) override {
    vr_context_->HandleInput(event);
  }
  void OnCloseRequest() override { Quit(); }
  void OnClosed() override {}
  void OnWindowStateChanged(ui::PlatformWindowState new_state) override {}
  void OnLostCapture() override {}
  void OnAcceleratedWidgetAvailable(gfx::AcceleratedWidget widget) override {
    DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
    widget_ = widget;
  }
  void OnAcceleratedWidgetDestroyed() override { NOTREACHED(); }
  void OnActivationChanged(bool active) override {}
  void OnMouseEnter() override {}

 private:
  // Since we pretend to have a GPU process, we should also pretend to
  // initialize the GPU resources via a posted task.
  void StartOnGpu() {
    renderer_ = renderer_factory_->CreateRenderer(GetAcceleratedWidget());
    vr_context_ = std::make_unique<vr::VrTestContext>(renderer_.get());
    renderer_->set_vr_context(vr_context_.get());
  }

  WindowManager* window_manager_;      // Not owned.
  RendererFactory* renderer_factory_;  // Not owned.

  std::unique_ptr<vr::VrTestContext> vr_context_;
  std::unique_ptr<vr::GlRenderer> renderer_;

  // Window-related state.
  std::unique_ptr<ui::PlatformWindowBase> platform_window_;
  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;

  base::WeakPtrFactory<AppWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppWindow);
};

RendererFactory::RendererFactory() {}

RendererFactory::~RendererFactory() {}

bool RendererFactory::Initialize() {
  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForGPU(params);

  if (gl::init::InitializeGLOneOff() &&
      gpu_helper_.Initialize(base::ThreadTaskRunnerHandle::Get())) {
    return true;
  }
  return false;
}

std::unique_ptr<vr::GlRenderer> RendererFactory::CreateRenderer(
    gfx::AcceleratedWidget widget) {
  scoped_refptr<gl::GLSurface> surface = gl::init::CreateViewGLSurface(widget);
  if (!surface) {
    LOG(FATAL) << "Failed to create GL surface";
    return nullptr;
  }
  auto renderer = std::make_unique<vr::GlRenderer>();
  CHECK(renderer->Initialize(surface));
  return renderer;
}

WindowManager::WindowManager(const base::Closure& quit_closure)
    : delegate_(
          ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate()),
      quit_closure_(quit_closure) {
  if (!renderer_factory_.Initialize()) {
    LOG(FATAL) << "Failed to initialize renderer factory";
  }

  DCHECK(delegate_);
  delegate_->AddObserver(this);
  delegate_->Initialize();
  OnConfigurationChanged();
}

WindowManager::~WindowManager() {
  if (delegate_) {
    delegate_->RemoveObserver(this);
  }
}

void WindowManager::Quit() {
  quit_closure_.Run();
}

void WindowManager::OnConfigurationChanged() {
  if (is_configuring_) {
    should_configure_ = true;
    return;
  }

  is_configuring_ = true;
  delegate_->GetDisplays(base::BindRepeating(&WindowManager::OnDisplaysAquired,
                                             base::Unretained(this)));
}

void WindowManager::OnDisplaySnapshotsInvalidated() {}

void WindowManager::OnDisplaysAquired(
    const std::vector<display::DisplaySnapshot*>& displays) {
  windows_.clear();

  gfx::Point origin;
  for (auto* display : displays) {
    if (!display->native_mode()) {
      LOG(ERROR) << "Display " << display->display_id()
                 << " doesn't have a native mode";
      continue;
    }

    delegate_->Configure(
        *display, display->native_mode(), origin,
        base::BindRepeating(&WindowManager::OnDisplayConfigured,
                            base::Unretained(this),
                            gfx::Rect(origin, display->native_mode()->size())));
    origin.Offset(display->native_mode()->size().width(), 0);
  }
  is_configuring_ = false;

  if (should_configure_) {
    should_configure_ = false;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&WindowManager::OnConfigurationChanged,
                                  base::Unretained(this)));
  }
}

void WindowManager::OnDisplayConfigured(const gfx::Rect& bounds, bool success) {
  if (success) {
    std::unique_ptr<AppWindow> window(
        new AppWindow(this, &renderer_factory_, bounds));
    window->Start();
    windows_.push_back(std::move(window));
  } else {
    LOG(ERROR) << "Failed to configure display at " << bounds.ToString();
  }
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kUseGL, gl::kGLImplementationEGLName);

  // Build UI thread task executor. This is used by platform
  // implementations for event polling & running background tasks.
  base::SingleThreadTaskExecutor main_task_executor(base::MessagePumpType::UI);
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams("VrUiViewer");

  ui::OzonePlatform::InitParams params;
  params.single_process = true;
  ui::OzonePlatform::InitializeForUI(params);
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetCurrentLayoutByName("us");
  ui::MaterialDesignController::Initialize();

  base::RunLoop run_loop;

  WindowManager window_manager(run_loop.QuitClosure());

  run_loop.Run();

  return 0;
}
