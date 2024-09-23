// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include <memory>
#include <numbers>
#include <string>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/environment/grid.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "device/base/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/paint_vector_icon.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/vr/elements/spinner.h"
#endif

namespace vr {

namespace {

template <typename V, typename C, typename S>
void BindColor(Model* model,
               V* view,
               C color,
               const std::string& color_string,
               S setter,
               const std::string& setter_string) {
  view->AddBinding(std::make_unique<Binding<SkColor>>(
      base::BindRepeating([](Model* m, C c) { return (m->color_scheme()).*c; },
                          base::Unretained(model), color),
#ifndef NDEBUG
      color_string,
#endif
      base::BindRepeating(
          [](V* v, S s, const SkColor& value) { (v->*s)(value); },
          base::Unretained(view), setter)
#ifndef NDEBUG
          ,
      setter_string
#endif
      ));
}

#define VR_BIND_COLOR(m, v, c, s) BindColor(m, v, c, #c, s, #s)

#define VR_BIND_VISIBILITY(v, c) \
  v->AddBinding(                 \
      VR_BIND_FUNC(bool, Model, model_, c, UiElement, v.get(), SetVisible));

template <typename T, typename... Args>
std::unique_ptr<T> Create(UiElementName name, DrawPhase phase, Args&&... args) {
  auto element = std::make_unique<T>(std::forward<Args>(args)...);
  element->SetName(name);
  element->SetDrawPhase(phase);
  return element;
}

std::unique_ptr<TransientElement> CreateTransientParent(UiElementName name,
                                                        int timeout_seconds,
                                                        bool animate_opacity) {
  auto element =
      std::make_unique<SimpleTransientElement>(base::Seconds(timeout_seconds));
  element->SetName(name);
  element->SetVisible(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  return element;
}

#if !BUILDFLAG(IS_ANDROID)
std::unique_ptr<UiElement> CreateSpacer(float width, float height) {
  auto spacer = Create<UiElement>(kNone, kPhaseNone);
  spacer->SetType(kTypeSpacer);
  spacer->SetSize(width, height);
  return spacer;
}
#endif

void BindIndicatorText(Model* model, Text* text, const IndicatorSpec& spec) {
  text->AddBinding(std::make_unique<Binding<std::pair<bool, bool>>>(
      VR_BIND_LAMBDA(
          [](Model* model, bool CapturingStateModel::*signal) {
            return std::make_pair(model->active_capturing.*signal,
                                  model->background_capturing.*signal);
          },
          base::Unretained(model), spec.signal),
      VR_BIND_LAMBDA(
          [](Text* view, int resource, int background_resource,
             int potential_resource, const std::pair<bool, bool>& value) {
            if (value.first)
              view->SetText(l10n_util::GetStringUTF16(resource));
            else if (value.second)
              view->SetText(l10n_util::GetStringUTF16(background_resource));
            else
              view->SetText(l10n_util::GetStringUTF16(potential_resource));
          },
          base::Unretained(text), spec.resource_string,
          spec.background_resource_string, spec.potential_resource_string)));
}

std::unique_ptr<UiElement> CreateWebVrIndicator(Model* model,
                                                IndicatorSpec spec,
                                                DrawPhase phase) {
  auto container = Create<Rect>(spec.webvr_name, phase);
  VR_BIND_COLOR(model, container.get(),
                &ColorScheme::webvr_permission_background, &Rect::SetColor);
  container->SetCornerRadius(kWebVrPermissionCornerRadius);
  container->set_bounds_contain_children(true);
  container->SetVisible(false);
  container->set_padding(
      kWebVrPermissionLeftPadding, kWebVrPermissionTopPadding,
      kWebVrPermissionRightPadding, kWebVrPermissionBottomPadding);

  auto layout = Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  layout->set_margin(kWebVrPermissionMargin);

  auto icon_element = Create<VectorIcon>(kNone, phase, 128);
  VR_BIND_COLOR(model, icon_element.get(),
                &ColorScheme::webvr_permission_foreground,
                &VectorIcon::SetColor);
  icon_element->set_y_anchoring(TOP);
  icon_element->SetSize(kWebVrPermissionIconSize, kWebVrPermissionIconSize);
  icon_element->SetIcon(*spec.icon);

  std::unique_ptr<UiElement> description_element;
  auto text_element = Create<Text>(kNone, phase, kWebVrPermissionFontHeight);
  text_element->SetColor(SK_ColorWHITE);
  text_element->SetFieldWidth(kWebVrPermissionTextWidth);
  if (spec.signal) {
    BindIndicatorText(model, text_element.get(), spec);
  } else {
    text_element->SetText(l10n_util::GetStringUTF16(spec.resource_string));
  }
  VR_BIND_COLOR(model, text_element.get(),
                &ColorScheme::webvr_permission_foreground, &Text::SetColor);
  description_element = std::move(text_element);

  layout->AddChild(std::move(icon_element));
  layout->AddChild(std::move(description_element));
  container->AddChild(std::move(layout));

  return container;
}

std::unique_ptr<Grid> CreateGrid(Model* model, UiElementName name) {
  auto grid = Create<Grid>(name, kPhaseBackground);
  grid->set_gridline_count(kFloorGridlineCount);
  return grid;
}

void ApplyFloorTransform(Rect* floor) {
  floor->SetSize(1.0f, 1.0f);
  floor->SetScale(kSceneSize, kSceneSize, kSceneSize);
  floor->SetTranslate(0.0, kFloorHeight, 0.0);
  floor->SetRotate(1, 0, 0, -std::numbers::pi_v<float> / 2);
}

void SetVisibleInLayout(UiElement* e, bool v) {
  e->SetVisible(v);
  e->set_requires_layout(v);
}

#if BUILDFLAG(IS_WIN)
void BindIndicatorTranscienceForWin(
    TransientElement* e,
    Model* model,
    UiScene* scene,
    const std::optional<
        std::tuple<bool, CapturingStateModel, CapturingStateModel>>& last_value,
    const std::tuple<bool, CapturingStateModel, CapturingStateModel>& value) {
  const bool in_web_vr_presentation = model->web_vr.IsImmersiveWebXrVisible() &&
                                      model->web_vr.has_received_permissions;

  const CapturingStateModel active_capture = std::get<1>(value);
  const CapturingStateModel potential_capture = std::get<2>(value);

  if (!in_web_vr_presentation) {
    e->SetVisibleImmediately(false);
    return;
  }

  e->SetVisible(true);
  e->RefreshVisible();

  for (const auto& spec : GetIndicatorSpecs()) {
    SetVisibleInLayout(
        scene->GetUiElementByName(spec.webvr_name),
        active_capture.*spec.signal || potential_capture.*spec.signal);
  }

  e->RemoveKeyframeModels(TRANSFORM);

  e->SetTranslate(0, kWebVrPermissionOffsetStart, 0);

  // Build up a keyframe model for the initial transition.
  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());

  gfx::TransformOperations value_1;
  value_1.AppendTranslate(0, kWebVrPermissionOffsetStart, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::TimeDelta(), value_1,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  gfx::TransformOperations value_2;
  value_2.AppendTranslate(0, kWebVrPermissionOffsetOvershoot, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::Milliseconds(kWebVrPermissionOffsetMs), value_2,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  gfx::TransformOperations value_3;
  value_3.AppendTranslate(0, kWebVrPermissionOffsetFinal, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::Milliseconds(kWebVrPermissionAnimationDurationMs), value_3,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  curve->set_target(e);

  e->AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      TRANSFORM));
}

#else

void BindIndicatorTranscience(TransientElement* e,
                              Model* model,
                              UiScene* scene,
                              const bool& in_web_vr_presentation) {
  if (!in_web_vr_presentation) {
    e->SetVisibleImmediately(false);
    return;
  }

  e->SetVisible(true);
  e->RefreshVisible();

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    SetVisibleInLayout(scene->GetUiElementByName(spec.webvr_name),
                       model->active_capturing.*spec.signal ||
                           model->potential_capturing.*spec.signal ||
                           model->background_capturing.*spec.signal);
  }

  e->RemoveKeyframeModels(TRANSFORM);
  e->SetTranslate(0, kWebVrPermissionOffsetStart, 0);

  // Build up a keyframe model for the initial transition.
  std::unique_ptr<gfx::KeyframedTransformAnimationCurve> curve(
      gfx::KeyframedTransformAnimationCurve::Create());

  gfx::TransformOperations value_1;
  value_1.AppendTranslate(0, kWebVrPermissionOffsetStart, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::TimeDelta(), value_1,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  gfx::TransformOperations value_2;
  value_2.AppendTranslate(0, kWebVrPermissionOffsetOvershoot, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::Milliseconds(kWebVrPermissionOffsetMs), value_2,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  gfx::TransformOperations value_3;
  value_3.AppendTranslate(0, kWebVrPermissionOffsetFinal, 0);
  curve->AddKeyframe(gfx::TransformKeyframe::Create(
      base::Milliseconds(kWebVrPermissionAnimationDurationMs), value_3,
      gfx::CubicBezierTimingFunction::CreatePreset(
          gfx::CubicBezierTimingFunction::EaseType::EASE)));

  curve->set_target(e);

  e->AddKeyframeModel(gfx::KeyframeModel::Create(
      std::move(curve), gfx::KeyframeEffect::GetNextKeyframeModelId(),
      TRANSFORM));
}

#endif

int GetIndicatorsTimeout() {
  // Some runtimes on Windows have quite lengthy animations that may cause
  // indicators to not be visible at our normal timeout length.
#if BUILDFLAG(IS_WIN)
  return kWindowsInitialIndicatorsTimeoutSeconds;
#else
  return kToastTimeoutSeconds;
#endif
}

}  // namespace

UiSceneCreator::UiSceneCreator(UiScene* scene, Ui* ui, Model* model)
    : scene_(scene), ui_(ui), model_(model) {}

UiSceneCreator::~UiSceneCreator() {}

void UiSceneCreator::CreateScene() {
  CreateWebVrRoot();
  CreateViewportAwareRoot();
  CreateWebVrSubtree();
}

void UiSceneCreator::CreateWebVrRoot() {
  auto element = std::make_unique<UiElement>();
  element->SetName(kWebVrRoot);
  element->SetVisible(true);
  element->SetTranslate(0, 0, 0);
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneCreator::CreateExternalPromptNotifcationOverlay() {
#if !BUILDFLAG(IS_ANDROID)
  auto phase = kPhaseForeground;
  auto icon = Create<VectorIcon>(kNone, phase, 100);
  icon->SetType(kTypePromptIcon);
  icon->SetSize(kPromptIconSize, kPromptIconSize);
  icon->set_y_anchoring(TOP);
  VR_BIND_COLOR(model_, icon.get(), &ColorScheme::modal_prompt_icon_foreground,
                &VectorIcon::SetColor);
  VectorIcon* vector_icon = icon.get();

  auto text1 = Create<Text>(kNone, phase, kPromptFontSize);
  text1->SetType(kTypePromptText);
  text1->SetFieldWidth(kPromptTextWidth);
  VR_BIND_COLOR(model_, text1.get(), &ColorScheme::modal_prompt_foreground,
                &Text::SetColor);
  Text* line1_text = text1.get();

  auto text2 = Create<Text>(kNone, phase, kPromptFontSize);
  text2->SetType(kTypePromptText);
  text2->SetFieldWidth(kPromptTextWidth);
  text2->SetText(l10n_util::GetStringUTF16(IDS_DESKTOP_PROMPT_DOFF_HEADSET));
  VR_BIND_COLOR(model_, text2.get(), &ColorScheme::modal_prompt_foreground,
                &Text::SetColor);

  // This spacer's padding ensures that the top line of text is aligned with the
  // icon even in the multi-line case.
  auto text_spacer1 = CreateSpacer(0, 0);
  text_spacer1->set_bounds_contain_children(true);
  text_spacer1->set_padding(0, (kPromptIconSize - kPromptFontSize) / 2, 0, 0);
  text_spacer1->AddChild(std::move(text1));

  // The second spacer gives space between the two strings.
  auto text_spacer2 = CreateSpacer(0, 0);
  text_spacer2->set_bounds_contain_children(true);
  text_spacer2->set_padding(0, kPromptFontSize * 2, 0, 0);
  text_spacer2->AddChild(std::move(text2));

  // Two lines of text:
  auto text_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kDown);
  text_layout->AddChild(std::move(text_spacer1));
  text_layout->AddChild(std::move(text_spacer2));

  // Contents of the message box.
  auto message_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  message_layout->set_margin(kPromptIconTextGap);
  message_layout->AddChild(std::move(icon));
  message_layout->AddChild(std::move(text_layout));

  auto prompt_window = Create<Rect>(kNone, phase);
  prompt_window->SetType(kTypePromptBackground);
  prompt_window->set_bounds_contain_children(true);
  prompt_window->set_padding(kPromptPadding, kPromptPadding);
  prompt_window->SetTranslate(0, 0, kPromptShadowOffsetDMM);
  prompt_window->SetCornerRadius(kPromptCornerRadius);
  prompt_window->AddChild(std::move(message_layout));
  VR_BIND_COLOR(model_, prompt_window.get(),
                &ColorScheme::modal_prompt_background, &Rect::SetColor);

  auto scaler = Create<ScaledDepthAdjuster>(kNone, kPhaseNone, kPromptDistance);
  scaler->SetName(kWebXrExternalPromptNotification);
  scaler->SetType(kTypeScaledDepthAdjuster);
  scaler->AddChild(std::move(prompt_window));
  scaler->set_contributes_to_parent_bounds(false);
  VR_BIND_VISIBILITY(scaler, (model->web_vr.external_prompt_notification !=
                              ExternalPromptNotificationType::kPromptNone));

  scaler->AddBinding(std::make_unique<Binding<ExternalPromptNotificationType>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->web_vr.external_prompt_notification; },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Text* text_element, VectorIcon* icon_element,
             const ExternalPromptNotificationType& prompt) {
            if (prompt == ExternalPromptNotificationType::kPromptNone)
              return;

            int message_id = 0;
            const gfx::VectorIcon* icon = nullptr;
            switch (prompt) {
              case ExternalPromptNotificationType::kPromptGenericPermission:
                message_id = IDS_VR_DESKTOP_GENERIC_PERMISSION_PROMPT;
                icon = &kOpenInBrowserIcon;
                break;
              case ExternalPromptNotificationType::kPromptNone:
                NOTREACHED_IN_MIGRATION();
            }

            text_element->SetText(l10n_util::GetStringUTF16(message_id));
            icon_element->SetIcon(icon);
          },
          base::Unretained(line1_text), base::Unretained(vector_icon))));

  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(scaler));
#endif  // !BUILDFLAG(IS_ANDROID)
}

void UiSceneCreator::CreateWebVrSubtree() {
  CreateWebVrOverlayElements();
  CreateWebVrTimeoutScreen();
  CreateExternalPromptNotifcationOverlay();

  // Note, this cannot be a descendant of the viewport aware root, otherwise it
  // will fade out when the viewport aware elements reposition.
  auto bg = std::make_unique<FullScreenRect>();
  bg->SetName(kWebVrBackground);
  bg->SetDrawPhase(kPhaseBackground);
  bg->SetVisible(false);
  bg->SetColor(model_->color_scheme().web_vr_background);
  bg->SetTransitionedProperties({OPACITY});
  VR_BIND_VISIBILITY(bg, (!model->web_vr.IsImmersiveWebXrVisible() ||
                          model->web_vr.external_prompt_notification !=
                              ExternalPromptNotificationType::kPromptNone));
  auto grid = CreateGrid(model_, kNone);
  grid->set_owner_name_for_test(kWebVrFloor);
  VR_BIND_COLOR(model_, grid.get(), &ColorScheme::web_vr_floor_grid,
                &Grid::SetGridColor);
  auto grid_bg = Create<Rect>(kWebVrFloor, kPhaseBackground);
  ApplyFloorTransform(grid_bg.get());
  VR_BIND_COLOR(model_, grid_bg.get(), &ColorScheme::web_vr_floor_center,
                &Rect::SetCenterColor);
  VR_BIND_COLOR(model_, grid_bg.get(), &ColorScheme::web_vr_floor_edge,
                &Rect::SetEdgeColor);
  grid_bg->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](UiElement* timeout_screen) {
            return timeout_screen->GetTargetOpacity() != 0.f;
          },
          base::Unretained(scene_->GetUiElementByName(kWebVrTimeoutRoot))),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(grid_bg.get()))));
  grid_bg->AddChild(std::move(grid));
  bg->AddChild(std::move(grid_bg));
  scene_->AddUiElement(kWebVrRoot, std::move(bg));
}

void UiSceneCreator::CreateWebVrTimeoutScreen() {
  auto scaler = std::make_unique<ScaledDepthAdjuster>(kTimeoutScreenDisatance);
  scaler->SetName(kWebVrTimeoutRoot);
  scaler->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model) {
            return (model->web_vr.state == kWebVrTimeoutImminent ||
                    model->web_vr.state == kWebVrTimedOut);
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) { e->SetVisible(value); },
          base::Unretained(scaler.get()))));

  // TODO(https://crbug.com/327467653): Investigate spinner code.
#if BUILDFLAG(IS_WIN)
  auto spinner = std::make_unique<Spinner>(512);
  spinner->SetName(kWebVrTimeoutSpinner);
  spinner->SetDrawPhase(kPhaseForeground);
  spinner->SetTransitionedProperties({OPACITY});
  spinner->SetVisible(false);
  spinner->SetSize(kTimeoutSpinnerSizeDMM, kTimeoutSpinnerSizeDMM);
  spinner->SetTranslate(0, kTimeoutSpinnerVerticalOffsetDMM, 0);
  spinner->SetColor(model_->color_scheme().web_vr_timeout_spinner);
  VR_BIND_VISIBILITY(spinner, model->web_vr.state == kWebVrTimeoutImminent);
#endif

  auto timeout_message = Create<Rect>(kWebVrTimeoutMessage, kPhaseForeground);
  timeout_message->SetVisible(false);
  timeout_message->set_bounds_contain_children(true);
  timeout_message->SetCornerRadius(kTimeoutMessageCornerRadiusDMM);
  timeout_message->SetTransitionedProperties({OPACITY, TRANSFORM});
  timeout_message->set_padding(kTimeoutMessageHorizontalPaddingDMM,
                               kTimeoutMessageVerticalPaddingDMM);
  VR_BIND_VISIBILITY(timeout_message, model->web_vr.state == kWebVrTimedOut);
  timeout_message->SetColor(
      model_->color_scheme().web_vr_timeout_message_background);

  auto timeout_layout = Create<LinearLayout>(kWebVrTimeoutMessageLayout,
                                             kPhaseNone, LinearLayout::kRight);
  timeout_layout->set_margin(kTimeoutMessageLayoutGapDMM);

  auto timeout_icon =
      Create<VectorIcon>(kWebVrTimeoutMessageIcon, kPhaseForeground, 512);
  timeout_icon->SetIcon(kSadTabIcon);
  timeout_icon->SetSize(kTimeoutMessageIconWidthDMM,
                        kTimeoutMessageIconHeightDMM);

  auto timeout_text = Create<Text>(kWebVrTimeoutMessageText, kPhaseForeground,
                                   kTimeoutMessageTextFontHeightDMM);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_TIMEOUT_MESSAGE));
  timeout_text->SetColor(
      model_->color_scheme().web_vr_timeout_message_foreground);
  timeout_text->SetFieldWidth(kTimeoutMessageTextWidthDMM);

  timeout_layout->AddChild(std::move(timeout_icon));
  timeout_layout->AddChild(std::move(timeout_text));
  timeout_message->AddChild(std::move(timeout_layout));

  scaler->AddChild(std::move(timeout_message));
#if BUILDFLAG(IS_WIN)
  scaler->AddChild(std::move(spinner));
#endif
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(scaler));
}
void UiSceneCreator::CreateViewportAwareRoot() {
  auto element = std::make_unique<ViewportAwareRoot>();
  element->SetName(kWebVrViewportAwareRoot);

  // On Windows, allow the viewport-aware UI to translate as well as rotate, so
  // it remains centered appropriately if the user moves.  Only enabled for
  // OS_WIN, since it conflicts with browser UI that isn't shown on Windows.
#if BUILDFLAG(IS_WIN)
  element->SetRecenterOnRotate(true);
#endif
  scene_->AddUiElement(kWebVrRoot, std::move(element));
}

void UiSceneCreator::CreateWebVrOverlayElements() {
  // Create transient WebVR elements.
  auto indicators = Create<LinearLayout>(kWebVrIndicatorLayout, kPhaseNone,
                                         LinearLayout::kDown);
  indicators->SetTranslate(0, 0, kWebVrPermissionDepth);
  indicators->set_margin(kWebVrPermissionOuterMargin);

  DrawPhase phase = kPhaseOverlayForeground;

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    indicators->AddChild(CreateWebVrIndicator(model_, spec, phase));
  }

  auto parent = CreateTransientParent(kWebVrIndicatorTransience,
                                      GetIndicatorsTimeout(), true);

#if BUILDFLAG(IS_WIN)
  parent->AddBinding(
      std::make_unique<
          Binding<std::tuple<bool, CapturingStateModel, CapturingStateModel>>>(
          VR_BIND_LAMBDA(
              [](Model* model) {
                return std::tuple<bool, CapturingStateModel,
                                  CapturingStateModel>(

                    model->web_vr.IsImmersiveWebXrVisible() &&
                        model->web_vr.has_received_permissions,
                    model->active_capturing, model->potential_capturing);
              },
              base::Unretained(model_)),
          VR_BIND_LAMBDA(BindIndicatorTranscienceForWin,
                         base::Unretained(parent.get()),
                         base::Unretained(model_), base::Unretained(scene_))));
#else

  parent->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* model) {
            return model->web_vr.IsImmersiveWebXrVisible() &&
                   model->web_vr.has_received_permissions;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(BindIndicatorTranscience, base::Unretained(parent.get()),
                     base::Unretained(model_), base::Unretained(scene_))));
#endif

  auto scaler = std::make_unique<ScaledDepthAdjuster>(kWebVrToastDistance);
  scaler->AddChild(std::move(indicators));
  parent->AddChild(std::move(scaler));

  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(parent));
}

}  // namespace vr
