// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/testapp/vr_test_context.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/i18n/icu_util.h"
#include "base/numerics/ranges.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/version.h"
#include "chrome/browser/vr/assets_load_status.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/testapp/assets_component_version.h"
#include "chrome/browser/vr/testapp/test_keyboard_delegate.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_unsupported_mode.h"
#include "chrome/grit/vr_testapp_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/security_state/core/security_state.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/angle_conversions.h"

namespace vr {

namespace {

// Constants related to scaling of the UI for zooming, to let us look at
// elements up close. The adjustment factor determines how much the zoom changes
// at each adjustment step.
constexpr float kDefaultViewScaleFactor = 1.2f;
constexpr float kMinViewScaleFactor = 0.5f;
constexpr float kMaxViewScaleFactor = 5.0f;
constexpr float kViewScaleAdjustmentFactor = 0.2f;
constexpr float kPageLoadTimeMilliseconds = 1000;

constexpr gfx::Point3F kDefaultLaserOrigin = {0.5f, -0.5f, 0.f};
constexpr gfx::Vector3dF kLaserLocalOffset = {0.f, -0.0075f, -0.05f};
constexpr float kControllerScaleFactor = 1.5f;
constexpr float kTouchpadPositionDelta = 0.05f;
const float kVerticalScrollScaleFactor =
    8.0f / ui::MouseWheelEvent::kWheelDelta;
const float kHorizontalScrollScaleFactor =
    100.0f / ui::MouseWheelEvent::kWheelDelta;
constexpr gfx::PointF kInitialTouchPosition = {0.5f, 0.5f};

void RotateToward(const gfx::Vector3dF& fwd, gfx::Transform* transform) {
  gfx::Quaternion quat(kForwardVector, fwd);
  transform->PreconcatTransform(gfx::Transform(quat));
}

bool LoadPng(int resource_id, std::unique_ptr<SkBitmap>* out_image) {
  base::StringPiece data =
      ui::ResourceBundle::GetSharedInstance().GetRawDataResource(resource_id);
  *out_image = std::make_unique<SkBitmap>();
  return gfx::PNGCodec::Decode(
      reinterpret_cast<const unsigned char*>(data.data()), data.size(),
      out_image->get());
}

InputEventList CreateScrollGestureEventList(InputEvent::Type type) {
  std::unique_ptr<InputEvent> gesture = std::make_unique<InputEvent>(type);
  InputEventList list;
  list.push_back(std::move(gesture));
  return list;
}

InputEventList CreateScrollGestureEventList(InputEvent::Type type,
                                            const gfx::Vector2dF& delta) {
  auto list = CreateScrollGestureEventList(type);
  InputEvent* event = static_cast<InputEvent*>(list.front().get());
  event->scroll_data.delta_x = delta.x();
  event->scroll_data.delta_y = delta.y();
  return list;
}

}  // namespace

VrTestContext::VrTestContext(GraphicsDelegate* graphics_delegate)
    : view_scale_factor_(kDefaultViewScaleFactor),
      graphics_delegate_(graphics_delegate) {
  base::FilePath pak_path;
  base::PathService::Get(base::DIR_MODULE, &pak_path);
  ui::ResourceBundle::InitSharedInstanceWithPakPath(
      pak_path.AppendASCII("vr_testapp.pak"));

  base::i18n::InitializeICU();

  auto text_input_delegate = std::make_unique<TextInputDelegate>();
  auto keyboard_delegate = std::make_unique<TestKeyboardDelegate>();
  keyboard_delegate_ = keyboard_delegate.get();
  text_input_delegate->SetUpdateInputCallback(
      base::BindRepeating(&TestKeyboardDelegate::UpdateInput,
                          base::Unretained(keyboard_delegate.get())));

  UiInitialState ui_initial_state;
  ui_instance_ = std::make_unique<Ui>(
      this, nullptr, std::move(keyboard_delegate),
      std::move(text_input_delegate), nullptr, ui_initial_state);
  ui_ = ui_instance_.get();

  LoadAssets();

  touchpad_touch_position_ = kInitialTouchPosition;

  model_ = ui_instance_->model_for_test();

  CycleOrigin();
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetHistoryButtonsEnabled(true, true);
  browser_ui->SetLoading(true);
  browser_ui->SetLoadProgress(0.4);
  CapturingStateModel active_capturing;
  CapturingStateModel background_capturing;
  CapturingStateModel potential_capturing;
  potential_capturing.video_capture_enabled = true;
  background_capturing.screen_capture_enabled = true;
  active_capturing.bluetooth_connected = true;
  active_capturing.location_access_enabled = true;
  browser_ui->SetCapturingState(active_capturing, background_capturing,
                                potential_capturing);
  ui_instance_->input_manager()->set_hit_test_strategy(
      UiInputManager::PROJECT_TO_LASER_ORIGIN_FOR_TEST);

  InitializeGl();
}

VrTestContext::~VrTestContext() = default;

void VrTestContext::InitializeGl() {
  unsigned int content_texture_id = CreateTexture(0xFF000080);
  unsigned int ui_texture_id = CreateTexture(0xFF008000);
  ui_->OnGlInitialized(kGlTextureLocationLocal, content_texture_id,
                       content_texture_id, ui_texture_id);
  keyboard_delegate_->Initialize(
      ui_instance_->scene()->SurfaceProviderForTesting(),
      ui_instance_->ui_element_renderer());
}

void VrTestContext::DrawFrame() {
  base::TimeTicks current_time = base::TimeTicks::Now();

  RenderInfo render_info = GetRenderInfo();

  // Update the render position of all UI elements (including desktop).
  ui_->OnBeginFrame(current_time, head_pose_);
  ui_->OnProjMatrixChanged(render_info.left_eye_model.proj_matrix);

  UpdateController(render_info, current_time);

  graphics_delegate_->RunInSkiaContext(
      base::BindOnce(&UiInterface::UpdateSceneTextures, base::Unretained(ui_)));

  auto load_progress = (current_time - page_load_start_).InMilliseconds() /
                       kPageLoadTimeMilliseconds;
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetLoading(load_progress < 1.0f);
  browser_ui->SetLoadProgress(std::min(load_progress, 1.0f));

  if (web_vr_mode_ && model_->web_vr.state == kWebVrPresenting &&
      webvr_frames_received_) {
    ui_->DrawWebVrOverlayForeground(render_info);
  } else {
    ui_->Draw(render_info);
  }
}

void VrTestContext::HandleInput(ui::Event* event) {
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  if (event->IsKeyEvent()) {
    if (event->type() != ui::ET_KEY_PRESSED) {
      return;
    }
    if (event->AsKeyEvent()->key_code() == ui::VKEY_CONTROL) {
      return;
    }
    if (!event->IsControlDown() && keyboard_delegate_->HandleInput(event)) {
      return;
    }
    switch (event->AsKeyEvent()->code()) {
      case ui::DomCode::ESCAPE:
        view_scale_factor_ = kDefaultViewScaleFactor;
        head_angle_x_degrees_ = 0;
        head_angle_y_degrees_ = 0;
        head_pose_ = gfx::Transform();
        break;
      case ui::DomCode::US_F:
        fullscreen_ = !fullscreen_;
        browser_ui->SetFullscreen(fullscreen_);
        break;
      case ui::DomCode::US_A:
        if (model_->platform_toast) {
          browser_ui->CancelPlatformToast();
        } else {
          browser_ui->ShowPlatformToast(base::UTF8ToUTF16("Downloading"));
        }
        break;
      case ui::DomCode::US_H:
        handedness_ = handedness_ == ControllerModel::kRightHanded
                          ? ControllerModel::kLeftHanded
                          : ControllerModel::kRightHanded;
        break;
      case ui::DomCode::US_I:
        incognito_ = !incognito_;
        browser_ui->SetIncognito(incognito_);
        break;
      case ui::DomCode::US_C:
        CycleIndicators();
        break;
      case ui::DomCode::US_D:
        ui_instance_->Dump(false);
        break;
      case ui::DomCode::US_B:
        ui_instance_->Dump(true);
        break;
      case ui::DomCode::US_V:
        CreateFakeVoiceSearchResult();
        break;
      case ui::DomCode::US_W:
        CycleWebVrModes();
        break;
      case ui::DomCode::US_S:
        ToggleSplashScreen();
        break;
      case ui::DomCode::US_R: {
        webvr_frames_received_ = true;
        CapturingStateModel active_capturing;
        CapturingStateModel background_capturing;
        CapturingStateModel potential_capturing;
        active_capturing.bluetooth_connected = true;
        active_capturing.location_access_enabled = true;
        browser_ui->SetCapturingState(active_capturing, background_capturing,
                                      potential_capturing);
        ui_->GetSchedulerUiPtr()->OnWebXrFrameAvailable();
        break;
      }
      case ui::DomCode::US_O:
        CycleOrigin();
        model_->can_navigate_back = !model_->can_navigate_back;
        break;
      case ui::DomCode::US_P:
        model_->toggle_mode(kModeRepositionWindow);
        break;
      case ui::DomCode::US_G:
        recentered_ = true;
        break;
      case ui::DomCode::US_T:
        touching_touchpad_ = !touching_touchpad_;
        break;
      case ui::DomCode::US_Q: {
        auto mode = model_->active_modal_prompt_type;
        model_->active_modal_prompt_type =
            static_cast<ModalPromptType>((mode + 1) % kNumModalPromptTypes);
        model_->push_mode(kModeModalPrompt);
        break;
      }
      case ui::DomCode::US_L:
        model_->standalone_vr_device = !model_->standalone_vr_device;
        break;
      case ui::DomCode::US_N:
        if (hosted_ui_enabled_) {
          CloseHostedDialog();
        } else {
          ui_->SetAlertDialogEnabled(true, nullptr, 100, 50);
          hosted_ui_enabled_ = true;
        }
        break;
      default:
        break;
    }
    return;
  }

  if (event->IsMouseWheelEvent()) {
    int direction =
        base::ClampToRange(event->AsMouseWheelEvent()->y_offset(), -1, 1);
    if (event->IsControlDown()) {
      view_scale_factor_ *= (1 + direction * kViewScaleAdjustmentFactor);
      view_scale_factor_ = base::ClampToRange(
          view_scale_factor_, kMinViewScaleFactor, kMaxViewScaleFactor);
    } else if (model_->reposition_window_enabled()) {
      touchpad_touch_position_.set_y(base::ClampToRange(
          touchpad_touch_position_.y() + kTouchpadPositionDelta * direction,
          0.0f, 1.0f));
    } else {
      input_event_lists_.push(
          CreateScrollGestureEventList(InputEvent::kScrollBegin));

      auto offset = gfx::Vector2dF(event->AsMouseWheelEvent()->offset());
      if (event->IsShiftDown()) {
        offset.Scale(kHorizontalScrollScaleFactor);
        offset = gfx::Vector2dF(offset.y(), offset.x());
      } else {
        offset.Scale(kVerticalScrollScaleFactor);
      }
      input_event_lists_.push(
          CreateScrollGestureEventList(InputEvent::kScrollUpdate, offset));

      input_event_lists_.push(
          CreateScrollGestureEventList(InputEvent::kScrollEnd));
    }
    return;
  }

  if (!event->IsMouseEvent()) {
    return;
  }

  const ui::MouseEvent* mouse_event = event->AsMouseEvent();

  if (mouse_event->IsMiddleMouseButton() &&
      mouse_event->type() == ui::ET_MOUSE_RELEASED) {
    InputEventList list;
    list.push_back(
        std::make_unique<InputEvent>(InputEvent::kMenuButtonClicked));
    input_event_lists_.push(std::move(list));
  }

  // TODO(cjgrant): Figure out why, quite regularly, mouse click events do not
  // make it into this method and are missed.
  if (mouse_event->IsLeftMouseButton()) {
    if (mouse_event->type() == ui::ET_MOUSE_PRESSED) {
      touchpad_pressed_ = true;
    } else if (mouse_event->type() == ui::ET_MOUSE_RELEASED) {
      touchpad_pressed_ = false;
    }
  }

  // Move the head pose if needed.
  if (mouse_event->IsRightMouseButton()) {
    if (last_drag_x_pixels_ != 0 || last_drag_y_pixels_ != 0) {
      float angle_y = 180.f *
                      ((mouse_event->x() - last_drag_x_pixels_) - 0.5f) /
                      window_size_.width();
      float angle_x = 180.f *
                      ((mouse_event->y() - last_drag_y_pixels_) - 0.5f) /
                      window_size_.height();
      head_angle_x_degrees_ += angle_x;
      head_angle_y_degrees_ += angle_y;
      head_angle_x_degrees_ =
          base::ClampToRange(head_angle_x_degrees_, -90.f, 90.f);
    }
    last_drag_x_pixels_ = mouse_event->x();
    last_drag_y_pixels_ = mouse_event->y();
  } else {
    last_drag_x_pixels_ = 0;
    last_drag_y_pixels_ = 0;
  }

  head_pose_ = gfx::Transform();
  head_pose_.RotateAboutXAxis(-head_angle_x_degrees_);
  head_pose_.RotateAboutYAxis(-head_angle_y_degrees_);

  last_mouse_point_ = gfx::Point(mouse_event->x(), mouse_event->y());
}

ControllerModel VrTestContext::UpdateController(const RenderInfo& render_info,
                                                base::TimeTicks current_time) {
  // We could map mouse position to controller position, and skip this logic,
  // but it will make targeting elements with a mouse feel strange and not
  // mouse-like. Instead, we make the reticle track the mouse position linearly
  // by working from reticle position backwards to compute controller position.
  // We also don't apply the elbow model (the controller pivots around its
  // centroid), so do not expect the positioning of the controller in the test
  // app to exactly match what will happen in production.
  //
  // We first set up a controller model that simulates firing the laser directly
  // through a screen pixel. We do this by computing two points behind the mouse
  // position in normalized device coordinates. The z components are arbitrary.
  gfx::Point3F mouse_point_far(
      2.0 * last_mouse_point_.x() / window_size_.width() - 1.0,
      -2.0 * last_mouse_point_.y() / window_size_.height() + 1.0, 0.8);
  gfx::Point3F mouse_point_near = mouse_point_far;
  mouse_point_near.set_z(-0.8);

  // We then convert these points into world space via the inverse view proj
  // matrix. These points are then used to construct a temporary controller
  // model that pretends to be shooting through the pixel at the mouse position.
  gfx::Transform inv_view_proj;
  CHECK(ViewProjectionMatrix().GetInverse(&inv_view_proj));
  inv_view_proj.TransformPoint(&mouse_point_near);
  inv_view_proj.TransformPoint(&mouse_point_far);

  ControllerModel controller_model;
  controller_model.touchpad_button_state =
      touchpad_pressed_ ? ControllerModel::ButtonState::kDown
                        : ControllerModel::ButtonState::kUp;
  controller_model.touchpad_touch_position = touchpad_touch_position_;
  controller_model.touching_touchpad = touching_touchpad_;
  controller_model.recentered = recentered_;
  recentered_ = false;

  controller_model.laser_origin = mouse_point_near;
  controller_model.laser_direction = mouse_point_far - mouse_point_near;
  CHECK(controller_model.laser_direction.GetNormalized(
      &controller_model.laser_direction));

  gfx::Point3F laser_origin = LaserOrigin();

  controller_model.transform.Translate3d(laser_origin.x(), laser_origin.y(),
                                         laser_origin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  // Hit testing is done in terms of this synthesized controller model.
  if (input_event_lists_.empty()) {
    input_event_lists_.push(InputEventList());
  }
  ReticleModel reticle_model;
  ui_->HandleInput(current_time, render_info, controller_model, &reticle_model,
                   &input_event_lists_.front());
  input_event_lists_.pop();

  // Now that we have accurate hit information, we use this to construct a
  // controller model for display.
  controller_model.laser_direction = reticle_model.target_point - laser_origin;

  controller_model.transform.MakeIdentity();
  controller_model.transform.Translate3d(laser_origin.x(), laser_origin.y(),
                                         laser_origin.z());
  controller_model.transform.Scale3d(
      kControllerScaleFactor, kControllerScaleFactor, kControllerScaleFactor);
  RotateToward(controller_model.laser_direction, &controller_model.transform);

  gfx::Vector3dF local_offset = kLaserLocalOffset;
  controller_model.transform.TransformVector(&local_offset);
  controller_model.laser_origin = laser_origin + local_offset;
  controller_model.handedness = handedness_;

  std::vector<ControllerModel> controllers;
  controllers.push_back(controller_model);
  ui_->OnControllersUpdated(controllers, reticle_model);

  return controller_model;
}

unsigned int VrTestContext::CreateTexture(SkColor color) {
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(1, 1);
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(color);

  SkPixmap pixmap;
  CHECK(surface->peekPixels(&pixmap));

  SkColorType type = pixmap.colorType();
  DCHECK(type == kRGBA_8888_SkColorType || type == kBGRA_8888_SkColorType);
  GLint format = (type == kRGBA_8888_SkColorType ? GL_RGBA : GL_BGRA);

  unsigned int texture_id;
  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, format, pixmap.width(), pixmap.height(), 0,
               format, GL_UNSIGNED_BYTE, pixmap.addr());

  return texture_id;
}

void VrTestContext::CreateFakeVoiceSearchResult() {
  if (!model_->voice_search_active())
    return;
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetRecognitionResult(
      base::UTF8ToUTF16("I would like to see cat videos, please."));
  browser_ui->SetSpeechRecognitionEnabled(false);
}

void VrTestContext::CycleWebVrModes() {
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  switch (model_->web_vr.state) {
    case kWebVrNoTimeoutPending: {
      web_vr_mode_ = true;
      webvr_frames_received_ = false;
      browser_ui->SetWebVrMode(true);
      break;
    }
    case kWebVrAwaitingFirstFrame:
      ui_->GetSchedulerUiPtr()->OnWebXrTimeoutImminent();
      break;
    case kWebVrTimeoutImminent:
      ui_->GetSchedulerUiPtr()->OnWebXrTimedOut();
      break;
    case kWebVrTimedOut:
      browser_ui->SetWebVrMode(false);
      web_vr_mode_ = false;
      break;
    case kWebVrPresenting:
      browser_ui->SetWebVrMode(false);
      web_vr_mode_ = false;
      break;
    default:
      break;
  }
}

void VrTestContext::ToggleSplashScreen() {
  if (!show_web_vr_splash_screen_) {
    web_vr_mode_ = true;
    webvr_frames_received_ = false;
    UiInitialState state;
    state.in_web_vr = true;
    ui_instance_->ReinitializeForTest(state);
  } else {
    ui_instance_->ReinitializeForTest(UiInitialState());
  }
  show_web_vr_splash_screen_ = !show_web_vr_splash_screen_;
}

gfx::Transform VrTestContext::ProjectionMatrix() const {
  gfx::Transform transform(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, -1, 0, 0, 0, -1, 0.5);
  if (window_size_.height() > 0) {
    transform.Scale(
        view_scale_factor_,
        view_scale_factor_ * window_size_.width() / window_size_.height());
  }
  return transform;
}

gfx::Transform VrTestContext::ViewProjectionMatrix() const {
  return ProjectionMatrix() * head_pose_;
}

void VrTestContext::SetVoiceSearchActive(bool active) {
  if (!voice_search_enabled_) {
    OnUnsupportedMode(
        UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission);
    return;
  }
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetSpeechRecognitionEnabled(active);
  if (active)
    browser_ui->OnSpeechRecognitionStateChanged(SPEECH_RECOGNITION_RECOGNIZING);
}

void VrTestContext::ExitPresent() {
  web_vr_mode_ = false;
  ui_->GetBrowserUiWeakPtr()->SetWebVrMode(false);
}

void VrTestContext::ExitFullscreen() {
  fullscreen_ = false;
  ui_->GetBrowserUiWeakPtr()->SetFullscreen(false);
}

void VrTestContext::Navigate(GURL gurl, NavigationMethod method) {
  LocationBarState state(gurl, security_state::SecurityLevel::WARNING,
                         &omnibox::kHttpIcon, true, false);
  ui_->GetBrowserUiWeakPtr()->SetLocationBarState(state);
  page_load_start_ = base::TimeTicks::Now();
}

void VrTestContext::NavigateBack() {
  page_load_start_ = base::TimeTicks::Now();
  model_->can_navigate_back = false;
  model_->can_navigate_forward = true;
}

void VrTestContext::NavigateForward() {
  page_load_start_ = base::TimeTicks::Now();
  model_->can_navigate_back = true;
  model_->can_navigate_forward = false;
}

void VrTestContext::ReloadTab() {
  page_load_start_ = base::TimeTicks::Now();
}

void VrTestContext::OpenNewTab(bool incognito) {
  incognito_ = incognito;
  auto browser_ui = ui_->GetBrowserUiWeakPtr();
  browser_ui->SetIncognito(incognito);
  model_->incognito_tabs_open = model_->incognito_tabs_open || incognito;
}

void VrTestContext::OpenBookmarks() {}
void VrTestContext::OpenRecentTabs() {}
void VrTestContext::OpenHistory() {}
void VrTestContext::OpenDownloads() {}
void VrTestContext::OpenShare() {}
void VrTestContext::OpenSettings() {}

void VrTestContext::CloseAllIncognitoTabs() {
  incognito_ = false;
  ui_->GetBrowserUiWeakPtr()->SetIncognito(false);
  model_->incognito_tabs_open = false;
}

void VrTestContext::OpenFeedback() {}

void VrTestContext::OnUnsupportedMode(vr::UiUnsupportedMode mode) {
  if (mode == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission) {
    ui_->GetBrowserUiWeakPtr()->ShowExitVrPrompt(mode);
  }
}

void VrTestContext::CloseHostedDialog() {
  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  hosted_ui_enabled_ = false;
}

void VrTestContext::OnExitVrPromptResult(vr::ExitVrPromptChoice choice,
                                         vr::UiUnsupportedMode reason) {
  DCHECK_NE(reason, UiUnsupportedMode::kCount);
  if (reason == UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission &&
      choice == CHOICE_EXIT) {
    voice_search_enabled_ = true;
  }
}

void VrTestContext::OnContentScreenBoundsChanged(const gfx::SizeF& bounds) {}

void VrTestContext::StartAutocomplete(const AutocompleteRequest& request) {
  std::vector<OmniboxSuggestion> result;
  auto browser_ui = ui_->GetBrowserUiWeakPtr();

  if (request.text.empty()) {
    browser_ui->SetOmniboxSuggestions(std::move(result));
    return;
  }

  // Supply an in-line match if the input matches a canned URL.
  base::string16 full_string = base::UTF8ToUTF16("wikipedia.org");
  if (!request.prevent_inline_autocomplete && request.text.size() >= 2 &&
      full_string.find(request.text) == 0) {
    result.emplace_back(full_string, base::string16(), ACMatchClassifications(),
                        ACMatchClassifications(), &vector_icons::kSearchIcon,
                        GURL(), request.text,
                        full_string.substr(request.text.size()));
  }

  // Supply a verbatim search match.
  result.emplace_back(request.text, base::string16(), ACMatchClassifications(),
                      ACMatchClassifications(), &vector_icons::kSearchIcon,
                      GURL(), base::string16(), base::string16());

  // Add a suggestion to exercise classification text styling.
  result.emplace_back(
      base::UTF8ToUTF16("Suggestion with classification"),
      base::UTF8ToUTF16("none url match dim"), ACMatchClassifications(),
      ACMatchClassifications{
          ACMatchClassification(0, ACMatchClassification::NONE),
          ACMatchClassification(5, ACMatchClassification::URL),
          ACMatchClassification(9, ACMatchClassification::MATCH),
          ACMatchClassification(15, ACMatchClassification::DIM),
      },
      &vector_icons::kSearchIcon, GURL("http://www.test.com/"),
      base::string16(), base::string16());

  while (result.size() < 4) {
    result.emplace_back(
        base::UTF8ToUTF16("Suggestion"),
        base::UTF8ToUTF16(
            "Very lengthy description of the suggestion that would wrap "
            "if not truncated through some other means."),
        ACMatchClassifications(), ACMatchClassifications(),
        &vector_icons::kSearchIcon, GURL("http://www.test.com/"),
        base::string16(), base::string16());
  }

  browser_ui->SetOmniboxSuggestions(std::move(result));
}

void VrTestContext::StopAutocomplete() {
  ui_->GetBrowserUiWeakPtr()->SetOmniboxSuggestions(
      std::vector<OmniboxSuggestion>{});
}

void VrTestContext::ShowPageInfo() {
  ui_->GetBrowserUiWeakPtr()->ShowExitVrPrompt(
      UiUnsupportedMode::kUnhandledPageInfo);
}

void VrTestContext::CycleIndicators() {
  static size_t state = 0;

  const std::vector<CapturingStateModelMemberPtr> signals = {
      &CapturingStateModel::location_access_enabled,
      &CapturingStateModel::audio_capture_enabled,
      &CapturingStateModel::video_capture_enabled,
      &CapturingStateModel::bluetooth_connected,
      &CapturingStateModel::screen_capture_enabled};

  state = (state + 1) % (1 << (signals.size() + 1));
  for (size_t i = 0; i < signals.size(); ++i) {
    model_->active_capturing.*signals[i] = state & (1 << i);
  }
}

void VrTestContext::CycleOrigin() {
  const std::vector<LocationBarState> states = {
      {GURL("http://domain.com"), security_state::SecurityLevel::WARNING,
       &omnibox::kHttpIcon, true, false},
      {GURL("https://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::SECURE, &omnibox::kHttpsValidIcon, true,
       false},
      {GURL("https://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::DANGEROUS,
       &omnibox::kNotSecureWarningIcon, true, false},
      // Do not show URL
      {GURL(), security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon,
       false, false},
      {GURL(), security_state::SecurityLevel::SECURE, &omnibox::kHttpsValidIcon,
       true, false},
      {GURL("file://very-very-very-long-file-hostname/path/path/path/path"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("file:///path/path/path/path/path/path/path/path/path"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      // Elision-related cases.
      {GURL("http://domaaaaaaaaaaain.com"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://domaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaain.com"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://domain.com/a/"), security_state::SecurityLevel::WARNING,
       &omnibox::kHttpIcon, true, false},
      {GURL("http://domain.com/aaaaaaa/"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://domain.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://domaaaaaaaaaaaaaaaaain.com/aaaaaaaaaaaaaaaaaaaaaaaaaaaaa/"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://domaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaain.com/aaaaaaaaaa/"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://www.domain.com/path/segment/directory/file.html"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://subdomain.domain.com/"),
       security_state::SecurityLevel::WARNING, &omnibox::kHttpIcon, true,
       false},
      {GURL("http://中央大学.ಠ_ಠ.tw/"), security_state::SecurityLevel::WARNING,
       &omnibox::kHttpIcon, true, false},
  };

  static int state = 0;
  ui_->GetBrowserUiWeakPtr()->SetLocationBarState(states[state]);
  state = (state + 1) % states.size();
}

RenderInfo VrTestContext::GetRenderInfo() const {
  RenderInfo render_info;
  render_info.head_pose = head_pose_;
  render_info.left_eye_model.viewport = gfx::Rect(window_size_);
  render_info.left_eye_model.view_matrix = head_pose_;
  render_info.left_eye_model.proj_matrix = ProjectionMatrix();
  render_info.left_eye_model.view_proj_matrix = ViewProjectionMatrix();
  render_info.right_eye_model = render_info.left_eye_model;
  return render_info;
}

gfx::Point3F VrTestContext::LaserOrigin() const {
  gfx::Point3F origin = kDefaultLaserOrigin;
  if (handedness_ == ControllerModel::kLeftHanded) {
    origin.set_x(-origin.x());
  }
  return origin;
}

void VrTestContext::LoadAssets() {
  base::Version assets_component_version(VR_ASSETS_COMPONENT_VERSION);
  auto assets = std::make_unique<Assets>();
  if (!(LoadPng(IDR_VR_BACKGROUND_IMAGE, &assets->background) &&
        LoadPng(IDR_VR_NORMAL_GRADIENT_IMAGE, &assets->normal_gradient) &&
        LoadPng(IDR_VR_INCOGNITO_GRADIENT_IMAGE, &assets->incognito_gradient) &&
        LoadPng(IDR_VR_FULLSCREEN_GRADIENT_IMAGE,
                &assets->fullscreen_gradient))) {
    ui_->GetBrowserUiWeakPtr()->OnAssetsLoaded(
        AssetsLoadStatus::kInvalidContent, nullptr, assets_component_version);
    return;
  }
  ui_->GetBrowserUiWeakPtr()->OnAssetsLoaded(
      AssetsLoadStatus::kSuccess, std::move(assets), assets_component_version);
}

}  // namespace vr
