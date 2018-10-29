// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
#define CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_

#include <memory>
#include <queue>

#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/model/controller_model.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_interface.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/transform.h"

namespace ui {
class Event;
}

namespace vr {

class GraphicsDelegate;
class TestKeyboardDelegate;
class Ui;
struct Model;

// This class provides a home for the VR UI in a testapp context, and
// manipulates the UI according to user input.
class VrTestContext : public vr::UiBrowserInterface {
 public:
  explicit VrTestContext(GraphicsDelegate* compositor_delgate);
  ~VrTestContext() override;

  // TODO(crbug/895313): Make use of BrowserRenderer.
  void DrawFrame();
  void HandleInput(ui::Event* event);

  // vr::UiBrowserInterface implementation (UI calling to VrShell).
  void ExitPresent() override;
  void ExitFullscreen() override;
  void NavigateBack() override;
  void NavigateForward() override;
  void ReloadTab() override;
  void OpenNewTab(bool incognito) override;
  void SelectTab(int id, bool incognito) override;
  void OpenBookmarks() override;
  void OpenRecentTabs() override;
  void OpenHistory() override;
  void OpenDownloads() override;
  void OpenShare() override;
  void OpenSettings() override;
  void CloseTab(int id, bool incognito) override;
  void CloseAllTabs() override;
  void CloseAllIncognitoTabs() override;
  void OpenFeedback() override;
  void CloseHostedDialog() override;
  void OnUnsupportedMode(vr::UiUnsupportedMode mode) override;
  void OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                            vr::UiUnsupportedMode reason) override;
  void OnContentScreenBoundsChanged(const gfx::SizeF& bounds) override;
  void SetVoiceSearchActive(bool active) override;
  void StartAutocomplete(const AutocompleteRequest& request) override;
  void StopAutocomplete() override;
  void ShowPageInfo() override;
  void Navigate(GURL gurl, NavigationMethod method) override;

  void set_window_size(const gfx::Size& size) { window_size_ = size; }

 private:
  void InitializeGl();
  unsigned int CreateTexture(SkColor color);
  void CreateFakeVoiceSearchResult();
  void CycleWebVrModes();
  void ToggleSplashScreen();
  void CycleOrigin();
  void CycleIndicators();
  RenderInfo GetRenderInfo() const;
  gfx::Transform ProjectionMatrix() const;
  gfx::Transform ViewProjectionMatrix() const;
  ControllerModel UpdateController(const RenderInfo& render_info,
                                   base::TimeTicks current_time);
  gfx::Point3F LaserOrigin() const;
  void LoadAssets();

  std::unique_ptr<Ui> ui_instance_;
  UiInterface* ui_;
  gfx::Size window_size_;

  gfx::Transform head_pose_;
  float head_angle_x_degrees_ = 0;
  float head_angle_y_degrees_ = 0;
  int last_drag_x_pixels_ = 0;
  int last_drag_y_pixels_ = 0;
  gfx::Point last_mouse_point_;
  bool touchpad_pressed_ = false;
  gfx::PointF touchpad_touch_position_;

  float view_scale_factor_ = 1.f;

  // This avoids storing a duplicate of the model state here.
  Model* model_;

  bool web_vr_mode_ = false;
  bool webvr_frames_received_ = false;
  bool fullscreen_ = false;
  bool incognito_ = false;
  bool show_web_vr_splash_screen_ = false;
  bool voice_search_enabled_ = false;
  bool touching_touchpad_ = false;
  bool recentered_ = false;
  base::TimeTicks page_load_start_;
  int tab_id_ = 0;
  bool hosted_ui_enabled_ = false;

  GraphicsDelegate* graphics_delegate_;
  TestKeyboardDelegate* keyboard_delegate_;

  ControllerModel::Handedness handedness_ = ControllerModel::kRightHanded;

  std::queue<InputEventList> input_event_lists_;

  DISALLOW_COPY_AND_ASSIGN(VrTestContext);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TESTAPP_VR_TEST_CONTEXT_H_
