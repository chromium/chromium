// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/ui_scene_creator.h"

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/i18n/case_conversion.h"
#include "base/numerics/math_constants.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_target.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframed_animation_curve.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/databinding/binding.h"
#include "chrome/browser/vr/databinding/vector_binding.h"
#include "chrome/browser/vr/elements/button.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/controller.h"
#include "chrome/browser/vr/elements/disc_button.h"
#include "chrome/browser/vr/elements/draw_phase.h"
#include "chrome/browser/vr/elements/environment/background.h"
#include "chrome/browser/vr/elements/environment/grid.h"
#include "chrome/browser/vr/elements/environment/stars.h"
#include "chrome/browser/vr/elements/full_screen_rect.h"
#include "chrome/browser/vr/elements/indicator_spec.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/laser.h"
#include "chrome/browser/vr/elements/linear_layout.h"
#include "chrome/browser/vr/elements/omnibox_formatting.h"
#include "chrome/browser/vr/elements/omnibox_text_field.h"
#include "chrome/browser/vr/elements/oval.h"
#include "chrome/browser/vr/elements/platform_ui_element.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/repositioner.h"
#include "chrome/browser/vr/elements/resizer.h"
#include "chrome/browser/vr/elements/reticle.h"
#include "chrome/browser/vr/elements/scaled_depth_adjuster.h"
#include "chrome/browser/vr/elements/scrollable_element.h"
#include "chrome/browser/vr/elements/spinner.h"
#include "chrome/browser/vr/elements/text.h"
#include "chrome/browser/vr/elements/text_button.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/elements/throbber.h"
#include "chrome/browser/vr/elements/transient_element.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/ui_texture.h"
#include "chrome/browser/vr/elements/url_text.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/elements/viewport_aware_root.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/platform_toast.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/target_property.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vector_icons/vector_icons.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/vector_icons/vector_icons.h"
#include "device/vr/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"

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

template <typename V, typename C, typename S>
void BindButtonColors(Model* model,
                      V* view,
                      C colors,
                      const std::string& colors_string,
                      S setter,
                      const std::string& setter_string) {
  view->AddBinding(std::make_unique<Binding<ButtonColors>>(
      base::BindRepeating([](Model* m, C c) { return (m->color_scheme()).*c; },
                          base::Unretained(model), colors),
#ifndef NDEBUG
      colors_string,
#endif
      base::BindRepeating(
          [](V* v, S s, const ButtonColors& value) { (v->*s)(value); },
          base::Unretained(view), setter)
#ifndef NDEBUG
          ,
      setter_string
#endif
      ));
}

#define VR_BIND_BUTTON_COLORS(m, v, c, s) BindButtonColors(m, v, c, #c, s, #s)

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

typedef VectorBinding<OmniboxSuggestion, Button> SuggestionSetBinding;
typedef typename SuggestionSetBinding::ElementBinding SuggestionBinding;

void OnSuggestionModelAdded(UiScene* scene,
                            UiBrowserInterface* browser,
                            Ui* ui,
                            Model* model,
                            AudioDelegate* audio_delegate,
                            SuggestionBinding* element_binding) {
  auto icon = std::make_unique<VectorIcon>(100);
  icon->SetDrawPhase(kPhaseForeground);
  icon->SetType(kTypeOmniboxSuggestionIcon);
  icon->SetSize(kSuggestionIconSizeDMM, kSuggestionIconSizeDMM);
  icon->AddBinding(VR_BIND_FUNC(SkColor, Model, model,
                                model->color_scheme().url_bar_button.foreground,
                                VectorIcon, icon.get(), SetColor));
  VectorIcon* p_icon = icon.get();

  auto icon_box = std::make_unique<UiElement>();
  icon_box->SetDrawPhase(kPhaseNone);
  icon_box->SetType(kTypeOmniboxSuggestionIconField);
  icon_box->set_hit_testable(true);
  icon_box->SetSize(kSuggestionIconFieldWidthDMM, kSuggestionHeightDMM);
  icon_box->AddChild(std::move(icon));

  auto content_text = std::make_unique<Text>(kSuggestionContentTextHeightDMM);
  content_text->SetDrawPhase(kPhaseForeground);
  content_text->SetType(kTypeOmniboxSuggestionContentText);
  content_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  content_text->SetFieldWidth(kSuggestionTextFieldWidthDMM);
  content_text->SetAlignment(kTextAlignmentLeft);
  Text* p_content_text = content_text.get();

  auto description_text =
      std::make_unique<Text>(kSuggestionDescriptionTextHeightDMM);
  description_text->SetDrawPhase(kPhaseForeground);
  description_text->SetType(kTypeOmniboxSuggestionDescriptionText);
  description_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  description_text->SetFieldWidth(kSuggestionTextFieldWidthDMM);
  description_text->SetAlignment(kTextAlignmentLeft);
  Text* p_description_text = description_text.get();

  auto text_layout = std::make_unique<LinearLayout>(LinearLayout::kDown);
  text_layout->SetType(kTypeOmniboxSuggestionTextLayout);
  text_layout->set_margin(kSuggestionLineGapDMM);
  text_layout->AddChild(std::move(content_text));
  text_layout->AddChild(std::move(description_text));

  auto right_margin = std::make_unique<UiElement>();
  right_margin->SetDrawPhase(kPhaseNone);
  right_margin->set_hit_testable(true);
  right_margin->SetSize(kSuggestionRightMarginDMM, kSuggestionHeightDMM);

  auto suggestion_layout = std::make_unique<LinearLayout>(LinearLayout::kRight);
  suggestion_layout->SetType(kTypeOmniboxSuggestionLayout);
  suggestion_layout->AddChild(std::move(icon_box));
  suggestion_layout->AddChild(std::move(text_layout));
  suggestion_layout->AddChild(std::move(right_margin));

  auto background = Create<Button>(
      kNone, kPhaseForeground,
      base::BindRepeating(
          [](UiBrowserInterface* b, Ui* ui, Model* m, SuggestionBinding* e) {
            b->Navigate(e->model()->destination,
                        NavigationMethod::kOmniboxSuggestionSelected);
            ui->OnUiRequestedNavigation();
          },
          base::Unretained(browser), base::Unretained(ui),
          base::Unretained(model), base::Unretained(element_binding)),
      audio_delegate);

  background->SetType(kTypeOmniboxSuggestionBackground);
  background->set_hit_testable(true);
  background->set_bubble_events(true);
  background->set_bounds_contain_children(true);
  background->set_hover_offset(0.0);
  VR_BIND_BUTTON_COLORS(model, background.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);
  background->AddChild(std::move(suggestion_layout));

  element_binding->bindings().push_back(
      VR_BIND_FUNC(base::string16, SuggestionBinding, element_binding,
                   model->model()->contents, Text, p_content_text, SetText));
  element_binding->bindings().push_back(
      std::make_unique<Binding<TextFormatting>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* suggestion, Model* model) {
                return ConvertClassification(
                    suggestion->model()->contents_classifications,
                    suggestion->model()->contents.size(),
                    model->color_scheme());
              },
              base::Unretained(element_binding), base::Unretained(model)),
          VR_BIND_LAMBDA(
              [](Text* v, const TextFormatting& formatting) {
                v->SetFormatting(formatting);
              },
              base::Unretained(p_content_text))));
  element_binding->bindings().push_back(
      std::make_unique<Binding<base::string16>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* m) { return m->model()->description; },
              base::Unretained(element_binding)),
          VR_BIND_LAMBDA(
              [](Text* v, const base::string16& text) {
                v->SetVisible(!text.empty());
                v->SetText(text);
              },
              base::Unretained(p_description_text))));
  element_binding->bindings().push_back(
      std::make_unique<Binding<TextFormatting>>(
          VR_BIND_LAMBDA(
              [](SuggestionBinding* suggestion, Model* model) {
                return ConvertClassification(
                    suggestion->model()->description_classifications,
                    suggestion->model()->description.size(),
                    model->color_scheme());
              },
              base::Unretained(element_binding), base::Unretained(model)),
          VR_BIND_LAMBDA(
              [](Text* v, const TextFormatting& formatting) {
                v->SetFormatting(formatting);
              },
              base::Unretained(p_description_text))));
  element_binding->bindings().push_back(
      VR_BIND(const gfx::VectorIcon*, SuggestionBinding, element_binding,
              model->model()->icon, VectorIcon, p_icon, view->SetIcon(*value)));
  element_binding->set_view(background.get());
  scene->AddUiElement(kOmniboxSuggestions, std::move(background));
}

void OnSuggestionModelRemoved(UiScene* scene, SuggestionBinding* binding) {
  scene->RemoveUiElement(binding->view()->id());
}

std::unique_ptr<TransientElement> CreateTransientParent(UiElementName name,
                                                        int timeout_seconds,
                                                        bool animate_opacity) {
  auto element = std::make_unique<SimpleTransientElement>(
      base::TimeDelta::FromSeconds(timeout_seconds));
  element->SetName(name);
  element->SetVisible(false);
  if (animate_opacity)
    element->SetTransitionedProperties({OPACITY});
  return element;
}

std::unique_ptr<UiElement> CreateSpacer(float width, float height) {
  auto spacer = Create<UiElement>(kNone, kPhaseNone);
  spacer->SetType(kTypeSpacer);
  spacer->SetSize(width, height);
  return spacer;
}

std::unique_ptr<UiElement> CreatePrompt(Model* model) {
  auto primary_button = Create<TextButton>(kNone, kPhaseForeground,
                                           kPromptButtonTextSize, nullptr);
  primary_button->SetType(kTypePromptPrimaryButton);
  primary_button->SetCornerRadius(kPromptButtonCornerRadius);
  VR_BIND_BUTTON_COLORS(model, primary_button.get(),
                        &ColorScheme::modal_prompt_primary_button_colors,
                        &Button::SetButtonColors);

  auto secondary_button = Create<TextButton>(kNone, kPhaseForeground,
                                             kPromptButtonTextSize, nullptr);
  secondary_button->SetType(kTypePromptSecondaryButton);
  secondary_button->SetCornerRadius(kPromptButtonCornerRadius);
  VR_BIND_BUTTON_COLORS(model, secondary_button.get(),
                        &ColorScheme::modal_prompt_secondary_button_colors,
                        &Button::SetButtonColors);

  auto button_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kLeft);
  button_layout->AddChild(std::move(primary_button));
  button_layout->AddChild(std::move(secondary_button));
  button_layout->set_margin(kPromptButtonGap);
  button_layout->set_x_anchoring(RIGHT);

  auto icon = Create<VectorIcon>(kNone, kPhaseForeground, 100);
  icon->SetType(kTypePromptIcon);
  icon->SetSize(kPromptIconSize, kPromptIconSize);
  icon->set_y_anchoring(TOP);
  VR_BIND_COLOR(model, icon.get(), &ColorScheme::modal_prompt_icon_foreground,
                &VectorIcon::SetColor);

  auto text = Create<Text>(kNone, kPhaseForeground, kPromptFontSize);
  text->SetType(kTypePromptText);
  text->SetLayoutMode(kMultiLineFixedWidth);
  text->SetAlignment(kTextAlignmentLeft);
  text->SetFieldWidth(kPromptTextWidth);
  VR_BIND_COLOR(model, text.get(), &ColorScheme::modal_prompt_foreground,
                &Text::SetColor);

  // This spacer's padding ensures that the top line of text is aligned with the
  // icon even in the multi-line case.
  auto text_spacer = CreateSpacer(0, 0);
  text_spacer->set_bounds_contain_children(true);
  text_spacer->set_padding(0, (kPromptIconSize - kPromptFontSize) / 2, 0, 0);
  text_spacer->set_y_anchoring(TOP);
  text_spacer->AddChild(std::move(text));

  auto message_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  message_layout->set_margin(kPromptIconTextGap);
  message_layout->AddChild(std::move(icon));
  message_layout->AddChild(std::move(text_spacer));

  auto prompt_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kDown);
  prompt_layout->set_margin(kPromptMessageButtonGap);
  prompt_layout->AddChild(std::move(message_layout));
  prompt_layout->AddChild(std::move(button_layout));

  auto background = Create<Rect>(kNone, kPhaseForeground);
  background->SetType(kTypePromptBackground);
  background->set_bounds_contain_children(true);
  background->set_hit_testable(true);
  background->set_padding(kPromptPadding, kPromptPadding);
  background->SetTranslate(0, 0, kPromptShadowOffsetDMM);
  background->SetCornerRadius(kPromptCornerRadius);
  background->AddChild(std::move(prompt_layout));
  VR_BIND_COLOR(model, background.get(), &ColorScheme::modal_prompt_background,
                &Rect::SetColor);

  auto shadow = Create<Shadow>(kNone, kPhaseForeground);
  shadow->SetType(kTypePromptShadow);
  shadow->AddChild(std::move(background));

  // Place an invisible but hittable plane behind the exit prompt, to keep the
  // reticle roughly planar with the content if near content.
  auto backplane = Create<InvisibleHitTarget>(kNone, kPhaseForeground);
  backplane->SetType(kTypePromptBackplane);
  backplane->SetTranslate(0, kPromptVerticalOffsetDMM, 0);
  backplane->SetSize(kBackplaneSize, kBackplaneSize);
  backplane->SetTransitionedProperties({OPACITY});
  backplane->AddChild(std::move(shadow));

  auto scaler = Create<ScaledDepthAdjuster>(kNone, kPhaseNone, kPromptDistance);
  scaler->SetType(kTypeScaledDepthAdjuster);
  scaler->AddChild(std::move(backplane));
  scaler->set_contributes_to_parent_bounds(false);
  return scaler;
}

base::RepeatingCallback<void()> CreatePromptCallback(
    UiUnsupportedMode mode,
    ExitVrPromptChoice choice,
    Model* model,
    UiBrowserInterface* browser) {
  return base::BindRepeating(
      [](UiUnsupportedMode mode, ExitVrPromptChoice choice, Model* m,
         UiBrowserInterface* b) {
        b->OnExitVrPromptResult(choice, mode);
        m->active_modal_prompt_type = kModalPromptTypeNone;
        m->pop_mode(kModeModalPrompt);
      },
      mode, choice, base::Unretained(model), base::Unretained(browser));
}

typedef VectorBinding<ControllerModel, Controller> ControllerSetBinding;
typedef typename ControllerSetBinding::ElementBinding ControllerBinding;

std::unique_ptr<UiElement> CreateControllerLabel(
    UiElementName name,
    float z_offset,
    const base::string16& text,
    Model* model,
    ControllerBinding* element_binding) {
  auto layout = Create<LinearLayout>(name, kPhaseNone, LinearLayout::kLeft);
  layout->set_margin(kControllerLabelLayoutMargin);
  layout->SetTranslate(0, 0, z_offset);
  layout->set_contributes_to_parent_bounds(false);
  layout->AddBinding(VR_BIND_FUNC(
      LayoutAlignment, ControllerBinding, element_binding,
      model->model()->handedness == ControllerModel::kRightHanded ? LEFT
                                                                  : RIGHT,
      LinearLayout, layout.get(), set_x_centering));
  layout->AddBinding(
      VR_BIND_FUNC(LinearLayout::Direction, ControllerBinding, element_binding,
                   model->model()->handedness == ControllerModel::kRightHanded
                       ? LinearLayout::kRight
                       : LinearLayout::kLeft,
                   LinearLayout, layout.get(), set_direction));

  auto spacer = std::make_unique<UiElement>();
  spacer->SetType(kTypeSpacer);
  spacer->SetVisible(true);
  spacer->set_requires_layout(true);
  spacer->SetSize(kControllerLabelSpacerSize, kControllerLabelSpacerSize);

  auto callout = Create<Rect>(kNone, kPhaseForeground);
  callout->SetVisible(true);
  callout->SetColor(SK_ColorWHITE);
  callout->SetSize(kControllerLabelCalloutWidth, kControllerLabelCalloutHeight);
  callout->SetRotate(1, 0, 0, -base::kPiFloat / 2);

  auto label =
      Create<Text>(kNone, kPhaseForeground, kControllerLabelFontHeight);
  label->SetText(text);
  label->SetColor(model->color_scheme().controller_label_callout);
  label->SetVisible(true);
  label->SetAlignment(kTextAlignmentRight);
  label->SetLayoutMode(kSingleLine);
  label->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  label->SetShadowsEnabled(true);
  label->SetScale(kControllerLabelScale, kControllerLabelScale,
                  kControllerLabelScale);

  layout->AddChild(std::move(spacer));
  layout->AddChild(std::move(callout));
  layout->AddChild(std::move(label));

  return layout;
}

std::unique_ptr<UiElement> CreateControllerElement(
    Model* model,
    ControllerBinding* element_binding) {
  auto controller = Create<Controller>(kController, kPhaseForeground);
  controller->AddBinding(VR_BIND_FUNC(gfx::Transform, ControllerBinding,
                                      element_binding,
                                      model->model()->transform, Controller,
                                      controller.get(), set_local_transform));
  controller->AddBinding(VR_BIND_FUNC(float, ControllerBinding, element_binding,
                                      model->model()->opacity, Controller,
                                      controller.get(), SetOpacity));

  auto touchpad_button =
      Create<Rect>(kControllerTouchpadButton, kPhaseForeground);
  touchpad_button->SetColor(model->color_scheme().controller_button);
  touchpad_button->SetSize(kControllerWidth, kControllerWidth);
  touchpad_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  touchpad_button->SetTranslate(0.0f, 0.0f,
                                -(kControllerLength - kControllerWidth) / 2);
  touchpad_button->SetCornerRadii({kControllerWidth / 2, kControllerWidth / 2,
                                   kControllerWidth / 2, kControllerWidth / 2});
  touchpad_button->AddBinding(std::make_unique<Binding<SkColor>>(
      VR_BIND_LAMBDA(
          [](ControllerBinding* m, Model* model) {
            return m->model()->touchpad_button_state ==
                           ControllerModel::ButtonState::kDown
                       ? model->color_scheme().controller_button_down
                       : model->color_scheme().controller_button;
          },
          base::Unretained(element_binding), base::Unretained(model)),
      VR_BIND_LAMBDA(
          [](Rect* touchpad_button, const SkColor& value) {
            touchpad_button->SetColor(value);
          },
          base::Unretained(touchpad_button.get()))));

  controller->AddChild(std::move(touchpad_button));

  auto app_button =
      Create<VectorIcon>(kControllerAppButton, kPhaseForeground, 100);
  app_button->SetIcon(kDaydreamControllerAppButtonIcon);
  app_button->SetColor(model->color_scheme().controller_button);
  app_button->SetSize(kControllerSmallButtonSize, kControllerSmallButtonSize);
  app_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  app_button->SetTranslate(0.0f, 0.0f, kControllerAppButtonZ);
  app_button->AddBinding(std::make_unique<Binding<SkColor>>(
      VR_BIND_LAMBDA(
          [](ControllerBinding* m, Model* model) {
            return m->model()->app_button_state ==
                           ControllerModel::ButtonState::kDown
                       ? model->color_scheme().controller_button_down
                       : model->color_scheme().controller_button;
          },
          base::Unretained(element_binding), base::Unretained(model)),
      VR_BIND_LAMBDA([](VectorIcon* app_button,
                        const SkColor& value) { app_button->SetColor(value); },
                     base::Unretained(app_button.get()))));

  controller->AddChild(std::move(app_button));

  auto home_button =
      Create<VectorIcon>(kControllerHomeButton, kPhaseForeground, 100);
  home_button->SetIcon(kDaydreamControllerHomeButtonIcon);
  home_button->SetColor(model->color_scheme().controller_button);
  home_button->SetSize(kControllerSmallButtonSize, kControllerSmallButtonSize);
  home_button->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  home_button->SetTranslate(0.0f, 0.0f, kControllerHomeButtonZ);
  home_button->AddBinding(std::make_unique<Binding<SkColor>>(
      VR_BIND_LAMBDA(
          [](ControllerBinding* m, Model* model) {
            return m->model()->home_button_state ==
                           ControllerModel::ButtonState::kDown
                       ? model->color_scheme().controller_button_down
                       : model->color_scheme().controller_button;
          },
          base::Unretained(element_binding), base::Unretained(model)),
      VR_BIND_LAMBDA([](VectorIcon* home_button,
                        const SkColor& value) { home_button->SetColor(value); },
                     base::Unretained(home_button.get()))));

  controller->AddChild(std::move(home_button));

  auto battery_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kRight);
  battery_layout->set_margin(kControllerBatteryDotMargin);
  battery_layout->SetRotate(1, 0, 0, -base::kPiFloat / 2);
  battery_layout->SetTranslate(0.0f, 0.0f, kControllerBatteryDotZ);

  for (int i = 0; i < kControllerBatteryDotCount; ++i) {
    auto battery_dot =
        Create<Rect>(static_cast<UiElementName>(kControllerBatteryDot0 + i),
                     kPhaseForeground);
    battery_dot->SetSize(kControllerBatteryDotSize, kControllerBatteryDotSize);
    battery_dot->SetCornerRadius(kControllerBatteryDotSize / 2);

    battery_dot->AddBinding(std::make_unique<Binding<SkColor>>(
        VR_BIND_LAMBDA(
            [](ControllerBinding* m, Model* model, int index) {
              return m->model()->battery_level > index
                         ? model->color_scheme().controller_battery_full
                         : model->color_scheme().controller_battery_empty;
            },
            base::Unretained(element_binding), base::Unretained(model), i),
        VR_BIND_LAMBDA(
            [](Rect* battery_dot, const SkColor& value) {
              battery_dot->SetColor(value);
            },
            base::Unretained(battery_dot.get()))));

    battery_layout->AddChild(std::move(battery_dot));
  }

  controller->AddChild(std::move(battery_layout));

  element_binding->set_view(controller.get());

  return controller;
}

void OnControllerModelAdded(UiScene* scene,
                            Model* model,
                            ControllerBinding* element_binding) {
  auto controller = CreateControllerElement(model, element_binding);

  auto callout_group = Create<UiElement>(kNone, kPhaseNone);
  callout_group->SetVisible(false);
  callout_group->SetTransitionedProperties({OPACITY});
  callout_group->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kControllerLabelTransitionDurationMs));
  callout_group->AddBinding(
      VR_BIND_FUNC(bool, ControllerBinding, element_binding,
                   model->model()->resting_in_viewport, UiElement,
                   callout_group.get(), SetVisible));

  auto trackpad_button =
      CreateControllerLabel(kControllerTrackpadLabel, kControllerTrackpadOffset,
                            l10n_util::GetStringUTF16(IDS_VR_BUTTON_TRACKPAD),
                            model, element_binding);
  trackpad_button->AddBinding(
      VR_BIND_FUNC(bool, Model, model, !model->reposition_window_enabled(),
                   UiElement, trackpad_button.get(), SetVisible));
  callout_group->AddChild(std::move(trackpad_button));

  auto reposition_button = CreateControllerLabel(
      kControllerTrackpadRepositionLabel, kControllerTrackpadOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_TRACKPAD_REPOSITION), model,
      element_binding);
  reposition_button->AddBinding(
      VR_BIND_FUNC(bool, Model, model, model->reposition_window_enabled(),
                   UiElement, reposition_button.get(), SetVisible));
  callout_group->AddChild(std::move(reposition_button));

  auto exit_button_label = CreateControllerLabel(
      kControllerExitButtonLabel, kControllerExitButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_EXIT), model, element_binding);
  exit_button_label->AddBinding(
      VR_BIND_FUNC(bool, Model, model, model->fullscreen_enabled(), UiElement,
                   exit_button_label.get(), SetVisible));
  callout_group->AddChild(std::move(exit_button_label));

  auto back_button_label = CreateControllerLabel(
      kControllerBackButtonLabel, kControllerBackButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_BACK), model, element_binding);
  back_button_label->AddBinding(VR_BIND_FUNC(
      bool, Model, model,
      model->omnibox_editing_enabled() || model->voice_search_active(),
      UiElement, back_button_label.get(), SetVisible));
  callout_group->AddChild(std::move(back_button_label));

  auto reposition_finish_button = CreateControllerLabel(
      kControllerRepositionFinishLabel, kControllerBackButtonOffset,
      l10n_util::GetStringUTF16(IDS_VR_BUTTON_APP_REPOSITION), model,
      element_binding);
  reposition_finish_button->AddBinding(
      VR_BIND_FUNC(bool, Model, model, model->reposition_window_enabled(),
                   UiElement, reposition_finish_button.get(), SetVisible));
  callout_group->AddChild(std::move(reposition_finish_button));

  controller->AddChild(std::move(callout_group));

  scene->AddUiElement(kControllerGroup, std::move(controller));
}

void OnControllerModelRemoved(UiScene* scene, ControllerBinding* binding) {
  scene->RemoveUiElement(binding->view()->id());
}

EventHandlers CreateRepositioningHandlers(Model* model, UiScene* scene) {
  EventHandlers handlers;
  handlers.button_down = base::BindRepeating(
      [](Model* model) { model->push_mode(kModeRepositionWindow); },
      base::Unretained(model));
  handlers.button_up = base::BindRepeating(
      [](Model* model, Repositioner* repositioner) {
        if (repositioner->HasMovedBeyondThreshold())
          model->pop_mode(kModeRepositionWindow);
      },
      base::Unretained(model),
      base::Unretained(static_cast<Repositioner*>(
          scene->GetUiElementByName(k2dBrowsingRepositioner))));
  return handlers;
}

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
                                                UiBrowserInterface* browser,
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
  if (spec.is_url) {
    icon_element->AddBinding(VR_BIND_FUNC(const gfx::VectorIcon*, Model, model,
                                          model->location_bar_state.vector_icon,
                                          VectorIcon, icon_element.get(),
                                          SetIcon));
  } else {
    icon_element->SetIcon(spec.icon);
  }

  std::unique_ptr<UiElement> description_element;
  if (spec.is_url) {
    auto url_text = Create<UrlText>(kNone, phase, kWebVrPermissionFontHeight);
    url_text->SetFieldWidth(kWebVrPermissionTextWidth);
    url_text->AddBinding(VR_BIND_FUNC(GURL, Model, model,
                                      model->location_bar_state.gurl, UrlText,
                                      url_text.get(), SetUrl));
    VR_BIND_COLOR(model, url_text.get(),
                  &ColorScheme::webvr_permission_foreground,
                  &UrlText::SetEmphasizedColor);
    VR_BIND_COLOR(model, url_text.get(),
                  &ColorScheme::webvr_permission_foreground,
                  &UrlText::SetDeemphasizedColor);
    description_element = std::move(url_text);

  } else {
    auto text_element = Create<Text>(kNone, phase, kWebVrPermissionFontHeight);
    text_element->SetLayoutMode(kMultiLineFixedWidth);
    text_element->SetAlignment(kTextAlignmentLeft);
    text_element->SetColor(SK_ColorWHITE);
    text_element->SetFieldWidth(kWebVrPermissionTextWidth);
    if (spec.signal)
      BindIndicatorText(model, text_element.get(), spec);
    else
      text_element->SetText(l10n_util::GetStringUTF16(spec.resource_string));
    VR_BIND_COLOR(model, text_element.get(),
                  &ColorScheme::webvr_permission_foreground, &Text::SetColor);
    description_element = std::move(text_element);
  }

  layout->AddChild(std::move(icon_element));
  layout->AddChild(std::move(description_element));
  container->AddChild(std::move(layout));

  return container;
}

std::unique_ptr<UiElement> CreateHostedUi(Model* model,
                                          UiBrowserInterface* browser,
                                          UiElementName name,
                                          UiElementName element_name,
                                          float distance) {
  auto hosted_ui = Create<PlatformUiElement>(element_name, kPhaseForeground);
  hosted_ui->SetSize(kContentWidth * kHostedUiWidthRatio,
                     kContentHeight * kHostedUiHeightRatio);
  // The hosted UI doesn't steal focus so that clikcing on an autofill
  // suggestion doesn't hide the keyboard. We will probably need to change this
  // when we support the keyboard on native UI elements.
  hosted_ui->set_focusable(false);
  hosted_ui->set_requires_layout(false);
  hosted_ui->SetCornerRadius(kContentCornerRadius);
  hosted_ui->SetTranslate(0, 0, kHostedUiShadowOffset);
  hosted_ui->AddBinding(VR_BIND_FUNC(PlatformUiInputDelegatePtr, Model, model,
                                     model->hosted_platform_ui.delegate,
                                     PlatformUiElement, hosted_ui.get(),
                                     SetDelegate));
  hosted_ui->AddBinding(VR_BIND_FUNC(
      unsigned int, Model, model, model->hosted_platform_ui.texture_id,
      PlatformUiElement, hosted_ui.get(), SetTextureId));
  hosted_ui->AddBinding(VR_BIND_FUNC(GlTextureLocation, Model, model,
                                     model->content_location, PlatformUiElement,
                                     hosted_ui.get(), SetTextureLocation));
  hosted_ui->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->hosted_platform_ui.hosted_ui_enabled; },
          base::Unretained(model)),
      VR_BIND_LAMBDA(
          [](PlatformUiElement* dialog, const bool& enabled) {
            dialog->set_requires_layout(enabled);
            dialog->set_hit_testable(enabled);
          },
          base::Unretained(hosted_ui.get()))));
  hosted_ui->AddBinding(
      VR_BIND(bool, Model, model, model->hosted_platform_ui.floating, UiElement,
              hosted_ui.get(),
              view->SetTranslate(0, 0, value ? 0 : kHostedUiShadowOffset)));
  hosted_ui->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::SizeF>>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return std::pair<bool, gfx::SizeF>(
                m->hosted_platform_ui.floating,
                gfx::SizeF(m->hosted_platform_ui.rect.width(),
                           m->hosted_platform_ui.rect.height()));
          },
          base::Unretained(model)),
      VR_BIND_LAMBDA(
          [](PlatformUiElement* dialog,
             const std::pair<bool, gfx::SizeF>& value) {
            if (!value.first && value.second.width() > 0) {
              float ratio = static_cast<float>(value.second.height()) /
                            value.second.width();
              dialog->SetSize(kContentWidth * kHostedUiWidthRatio,
                              kContentWidth * kHostedUiWidthRatio * ratio);
            } else if (value.first) {
              dialog->SetSize(kContentWidth * value.second.width(),
                              kContentWidth * value.second.height());
            }
          },
          base::Unretained(hosted_ui.get()))));

  auto shadow = Create<Shadow>(kNone, kPhaseForeground);
  shadow->SetType(kTypePromptShadow);
  shadow->SetTranslate(0, 0, kHostedUiDepthOffset - kHostedUiShadowOffset);
  shadow->SetVisible(false);
  shadow->set_opacity_when_visible(1.0);
  shadow->set_contributes_to_parent_bounds(false);
  shadow->SetTransitionedProperties({OPACITY});
  shadow->AddChild(std::move(hosted_ui));
  shadow->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::PointF>>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return std::pair<bool, gfx::PointF>(
                m->hosted_platform_ui.floating,
                gfx::PointF(m->hosted_platform_ui.rect.x(),
                            m->hosted_platform_ui.rect.y()));
          },
          base::Unretained(model)),
      VR_BIND_LAMBDA(
          [](Shadow* shadow, const std::pair<bool, gfx::PointF>& value) {
            if (value.first /* floating */) {
              shadow->set_x_centering(LEFT);
              shadow->set_y_centering(TOP);
              shadow->SetTranslate((value.second.x() - 0.5) * kContentWidth,
                                   (0.5 - value.second.y()) * kContentHeight,
                                   kFloatingHostedUiDistance);
              shadow->set_intensity(0);
            } else {
              shadow->set_x_centering(NONE);
              shadow->set_y_centering(NONE);
              shadow->SetTranslate(
                  0, 0, kHostedUiDepthOffset - kHostedUiShadowOffset);
              shadow->set_intensity(1);
            }
          },
          base::Unretained(shadow.get()))));
  shadow->AddBinding(VR_BIND_FUNC(bool, Model, model,
                                  model->hosted_platform_ui.hosted_ui_enabled,
                                  Shadow, shadow.get(), SetVisible));

  auto backplane = Create<InvisibleHitTarget>(name, kPhaseForeground);
  backplane->SetType(kTypeHostedUiBackplane);
  backplane->SetSize(kSceneSize, kSceneSize);
  backplane->SetTranslate(0.0, kContentVerticalOffset, -kContentDistance);
  backplane->set_contributes_to_parent_bounds(false);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->hosted_platform_ui.hosted_ui_enabled) {
          browser->CloseHostedDialog();
        }
      },
      base::Unretained(model), base::Unretained(browser));
  backplane->set_event_handlers(event_handlers);
  backplane->AddChild(std::move(shadow));
  backplane->AddBinding(
      VR_BIND_FUNC(bool, Model, model,
                   model->hosted_platform_ui.hosted_ui_enabled &&
                       model->active_modal_prompt_type == kModalPromptTypeNone,
                   InvisibleHitTarget, backplane.get(), SetVisible));

  return backplane;
}

std::unique_ptr<Grid> CreateGrid(Model* model, UiElementName name) {
  auto grid = Create<Grid>(name, kPhaseBackground);
  grid->set_gridline_count(kFloorGridlineCount);
  grid->set_hit_testable(true);
  grid->set_focusable(false);
  return grid;
}

void ApplyFloorTransform(Rect* floor) {
  floor->SetSize(1.0f, 1.0f);
  floor->SetScale(kSceneSize, kSceneSize, kSceneSize);
  floor->SetTranslate(0.0, kFloorHeight, 0.0);
  floor->SetRotate(1, 0, 0, -base::kPiFloat / 2);
}

void SetVisibleInLayout(UiElement* e, bool v) {
  e->SetVisible(v);
  e->set_requires_layout(v);
}

std::unique_ptr<TransientElement> CreateTextToast(
    UiElementName transient_parent_name,
    UiElementName toast_name,
    Model* model,
    const base::string16& text) {
  auto parent =
      CreateTransientParent(transient_parent_name, kToastTimeoutSeconds, false);
  parent->set_bounds_contain_children(true);
  parent->SetScale(kContentDistance, kContentDistance, 1.0f);

  auto background_element = Create<Rect>(toast_name, kPhaseForeground);
  VR_BIND_COLOR(model, background_element.get(), &ColorScheme::toast_background,
                &Rect::SetColor);

  background_element->set_bounds_contain_children(true);
  background_element->set_padding(kToastXPaddingDMM, kToastYPaddingDMM,
                                  kToastXPaddingDMM, kToastYPaddingDMM);
  background_element->SetTransitionedProperties({OPACITY});
  background_element->SetType(kTypeToastBackground);
  background_element->SetCornerRadius(kToastCornerRadiusDMM);

  auto text_element =
      Create<Text>(kNone, kPhaseForeground, kToastTextFontHeightDMM);
  text_element->SetLayoutMode(kSingleLine);
  text_element->SetColor(SK_ColorWHITE);
  text_element->set_owner_name_for_test(toast_name);
  text_element->SetType(kTypeToastText);
  text_element->SetText(text);

  VR_BIND_COLOR(model, text_element.get(), &ColorScheme::toast_foreground,
                &Text::SetColor);

  background_element->AddChild(std::move(text_element));
  parent->AddChild(std::move(background_element));
  return parent;
}

#if defined(OS_WIN)
void BindIndicatorTranscienceForWin(
    TransientElement* e,
    Model* model,
    UiScene* scene,
    const base::Optional<
        std::tuple<bool, CapturingStateModel, CapturingStateModel>>& last_value,
    const std::tuple<bool, CapturingStateModel, CapturingStateModel>& value) {
  const bool in_web_vr_presentation = model->web_vr_enabled() &&
                                      model->web_vr.IsImmersiveWebXrVisible() &&
                                      model->web_vr.has_received_permissions;

  const CapturingStateModel active_capture = std::get<1>(value);
  const CapturingStateModel potential_capture = std::get<2>(value);

  if (!in_web_vr_presentation) {
    e->SetVisibleImmediately(false);
    return;
  }

  e->SetVisible(true);
  e->RefreshVisible();

  SetVisibleInLayout(scene->GetUiElementByName(kWebVrExclusiveScreenToast),
                     !model->browsing_disabled);

  for (const auto& spec : GetIndicatorSpecs()) {
    SetVisibleInLayout(
        scene->GetUiElementByName(spec.webvr_name),
        active_capture.*spec.signal || potential_capture.*spec.signal);
  }

  e->RemoveKeyframeModels(TRANSFORM);

  e->SetTranslate(0, kWebVrPermissionOffsetStart, 0);

  // Build up a keyframe model for the initial transition.
  std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
      cc::KeyframedTransformAnimationCurve::Create());

  cc::TransformOperations value_1;
  value_1.AppendTranslate(0, kWebVrPermissionOffsetStart, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta(), value_1,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  cc::TransformOperations value_2;
  value_2.AppendTranslate(0, kWebVrPermissionOffsetOvershoot, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta::FromMilliseconds(kWebVrPermissionOffsetMs), value_2,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  cc::TransformOperations value_3;
  value_3.AppendTranslate(0, kWebVrPermissionOffsetFinal, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta::FromMilliseconds(kWebVrPermissionAnimationDurationMs),
      value_3,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  e->AddKeyframeModel(cc::KeyframeModel::Create(
      std::move(curve), Animation::GetNextKeyframeModelId(),
      Animation::GetNextGroupId(), TRANSFORM));
}

#else

void BindIndicatorTranscience(
    TransientElement* e,
    Model* model,
    UiScene* scene,
    const base::Optional<std::tuple<bool, bool, bool>>& last_value,
    const std::tuple<bool, bool, bool>& value) {
  const bool in_web_vr_presentation = std::get<0>(value);
  const bool in_long_press = std::get<1>(value);
  const bool showing_hosted_ui = std::get<2>(value);
  const bool was_in_long_press = last_value && std::get<1>(last_value.value());
  const bool was_showing_hosted_ui =
      last_value && std::get<2>(last_value.value());

  if (!in_web_vr_presentation) {
    e->SetVisibleImmediately(false);
    return;
  }

  // The reason we need the previous state is to disguish the
  // situation where the app button has been released after a long
  // press, and the situation when we want to initially show the
  // indicators.
  if (was_in_long_press && !in_long_press)
    return;

  // Similarly, we need to know when we've finished presenting hosted
  // ui because we should not show indicators then.
  if (was_showing_hosted_ui && !showing_hosted_ui)
    return;

  e->SetVisible(true);
  e->RefreshVisible();
  SetVisibleInLayout(scene->GetUiElementByName(kWebVrExclusiveScreenToast),
                     !model->browsing_disabled && !in_long_press);

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    SetVisibleInLayout(scene->GetUiElementByName(spec.webvr_name),
                       model->active_capturing.*spec.signal ||
                           model->potential_capturing.*spec.signal ||
                           model->background_capturing.*spec.signal);
  }

  e->RemoveKeyframeModels(TRANSFORM);
  if (in_long_press) {
    // We do not do a translation animation for long press.
    e->SetTranslate(0, 0, 0);
    return;
  }

  e->SetTranslate(0, kWebVrPermissionOffsetStart, 0);

  // Build up a keyframe model for the initial transition.
  std::unique_ptr<cc::KeyframedTransformAnimationCurve> curve(
      cc::KeyframedTransformAnimationCurve::Create());

  cc::TransformOperations value_1;
  value_1.AppendTranslate(0, kWebVrPermissionOffsetStart, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta(), value_1,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  cc::TransformOperations value_2;
  value_2.AppendTranslate(0, kWebVrPermissionOffsetOvershoot, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta::FromMilliseconds(kWebVrPermissionOffsetMs), value_2,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  cc::TransformOperations value_3;
  value_3.AppendTranslate(0, kWebVrPermissionOffsetFinal, 0);
  curve->AddKeyframe(cc::TransformKeyframe::Create(
      base::TimeDelta::FromMilliseconds(kWebVrPermissionAnimationDurationMs),
      value_3,
      cc::CubicBezierTimingFunction::CreatePreset(
          cc::CubicBezierTimingFunction::EaseType::EASE)));

  e->AddKeyframeModel(cc::KeyframeModel::Create(
      std::move(curve), Animation::GetNextKeyframeModelId(),
      Animation::GetNextGroupId(), TRANSFORM));
}

#endif

int GetIndicatorsTimeout() {
#if BUILDFLAG(ENABLE_WINDOWS_MR)
  if (base::FeatureList::IsEnabled(features::kWindowsMixedReality))
    return kWmrInitialIndicatorsTimeoutSeconds;
#endif
  return kToastTimeoutSeconds;
}

NOINLINE void CrashIntentionally() {
  LOG(ERROR) << "Crashing VR browser";

  static int static_variable_to_make_this_function_unique = 0;
  base::debug::Alias(&static_variable_to_make_this_function_unique);

  volatile int* zero = nullptr;
  *zero = 0;
}

}  // namespace

UiSceneCreator::UiSceneCreator(UiBrowserInterface* browser,
                               UiScene* scene,
                               Ui* ui,
                               ContentInputDelegate* content_input_delegate,
                               KeyboardDelegate* keyboard_delegate,
                               TextInputDelegate* text_input_delegate,
                               AudioDelegate* audio_delegate,
                               Model* model)
    : browser_(browser),
      scene_(scene),
      ui_(ui),
      content_input_delegate_(content_input_delegate),
      keyboard_delegate_(keyboard_delegate),
      text_input_delegate_(text_input_delegate),
      audio_delegate_(audio_delegate),
      model_(model) {}

UiSceneCreator::~UiSceneCreator() {}

void UiSceneCreator::CreateScene() {
  Create2dBrowsingSubtreeRoots();
  CreateWebVrRoot();
  CreateBackground();
  CreateViewportAwareRoot();
  CreateContentQuad();
  Create2dBrowsingHostedUi();
  CreatePrompts();
  CreateSystemIndicators();
  CreateUrlBar();
  CreateOverflowMenu();
  CreateOmnibox();
  CreateCloseButton();
  CreateToasts();
  CreateVoiceSearchUiGroup();
  CreateContentRepositioningAffordance();
  CreateWebVrSubtree();
  CreateKeyboard();
  CreateControllers();
}

void UiSceneCreator::Create2dBrowsingHostedUi() {
  auto hosted_ui_root =
      CreateHostedUi(model_, browser_, k2dBrowsingHostedUi,
                     k2dBrowsingHostedUiContent, kContentDistance);
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(hosted_ui_root));
}

void UiSceneCreator::Create2dBrowsingSubtreeRoots() {
  auto element = Create<UiElement>(k2dBrowsingRoot, kPhaseNone);
  element->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return m->browsing_enabled() && !m->waiting_for_background;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](UiElement* e, const bool& v) { e->SetVisible(v); },
                     base::Unretained(element.get()))));
  element->AddBinding(VR_BIND(
      float, Model, model_, model->floor_height, UiElement, element.get(),
      view->SetTranslate(0, value ? value - kFloorHeight : 0.0, 0.0)));

  scene_->AddUiElement(kRoot, std::move(element));

  element = Create<UiElement>(k2dBrowsingBackground, kPhaseNone);
  scene_->AddUiElement(k2dBrowsingRoot, std::move(element));

  auto repositioner = Create<Repositioner>(k2dBrowsingRepositioner, kPhaseNone);
  repositioner->set_bounds_contain_children(true);
  repositioner->AddBinding(
      VR_BIND_FUNC(bool, Model, model_, model->reposition_window_enabled(),
                   Repositioner, repositioner.get(), SetEnabled));
  repositioner->AddBinding(
      VR_BIND_FUNC(gfx::Vector3dF, Model, model_,
                   model->primary_controller().laser_direction, Repositioner,
                   repositioner.get(), set_laser_direction));
  repositioner->AddBinding(VR_BIND(
      bool, Model, model_, model->primary_controller().recentered, Repositioner,
      repositioner.get(), if (value) { view->Reset(); }));
  scene_->AddUiElement(k2dBrowsingRoot, std::move(repositioner));

  auto hider = Create<UiElement>(k2dBrowsingVisibiltyHider, kPhaseNone);
  hider->SetTransitionedProperties({OPACITY});
  hider->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  VR_BIND_VISIBILITY(
      hider, model->default_browsing_enabled() || model->fullscreen_enabled());
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(hider));

  auto fader = Create<UiElement>(k2dBrowsingVisibiltyFader, kPhaseNone);
  fader->SetTransitionedProperties({OPACITY});
  fader->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  fader->AddBinding(std::make_unique<Binding<float>>(
      VR_BIND_LAMBDA(
          [](Model* model) {
            if (model->has_mode_in_stack(kModeModalPrompt) ||
                (model->hosted_platform_ui.hosted_ui_enabled &&
                 !model->hosted_platform_ui.floating)) {
              return kModalPromptFadeOpacity;
            }
            return 1.0f;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const float& value) { e->SetOpacity(value); },
          base::Unretained(fader.get()))));
  scene_->AddUiElement(k2dBrowsingVisibiltyHider, std::move(fader));

  element = Create<UiElement>(k2dBrowsingForeground, kPhaseNone);
  element->set_bounds_contain_children(true);
  element->SetTransitionedProperties({OPACITY});
  element->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  scene_->AddUiElement(k2dBrowsingVisibiltyFader, std::move(element));

  element = Create<UiElement>(k2dBrowsingContentGroup, kPhaseNone);
  element->SetTranslate(0, kContentVerticalOffset, -kContentDistance);
  element->SetTransitionedProperties({TRANSFORM});
  element->set_bounds_contain_children(true);
  element->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              element.get(),
              view->SetTranslate(
                  0, value ? kFullscreenVerticalOffset : kContentVerticalOffset,
                  value ? -kFullscreenDistance : -kContentDistance)));
  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneCreator::CreateWebVrRoot() {
  auto element = std::make_unique<UiElement>();
  element->SetName(kWebVrRoot);
  VR_BIND_VISIBILITY(element, model->web_vr_enabled());
  element->AddBinding(VR_BIND(
      float, Model, model_, model->floor_height, UiElement, element.get(),
      view->SetTranslate(0, value ? value - kFloorHeight : 0.0, 0.0)));
  scene_->AddUiElement(kRoot, std::move(element));
}

void UiSceneCreator::CreateSystemIndicators() {
  auto backplane =
      Create<InvisibleHitTarget>(kIndicatorBackplane, kPhaseForeground);
  backplane->set_bounds_contain_children(true);
  backplane->set_contributes_to_parent_bounds(false);
  backplane->set_y_anchoring(TOP);
  backplane->SetCornerRadius(kIndicatorCornerRadiusDMM);
  backplane->SetTranslate(0, kIndicatorVerticalOffset,
                          kIndicatorDistanceOffset);
  backplane->SetScale(kIndicatorDepth, kIndicatorDepth, 1.0f);
  VR_BIND_VISIBILITY(backplane, !model->fullscreen_enabled());

  auto indicator_layout =
      Create<LinearLayout>(kIndicatorLayout, kPhaseNone, LinearLayout::kRight);
  indicator_layout->set_margin(kIndicatorMarginDMM);
  indicator_layout->set_contributes_to_parent_bounds(false);

  auto* content_frame = scene_->GetUiElementByName(kContentFrame);
  content_frame->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](UiElement* plane, UiElement* indicators) {
            if (static_cast<InvisibleHitTarget*>(plane)->hovered())
              return true;
            for (auto& child : indicators->children()) {
              if (static_cast<Button*>(child.get())->hovered())
                return true;
            }
            return false;
          },
          base::Unretained(scene_->GetUiElementByName(kContentFrameHitPlane)),
          base::Unretained(indicator_layout.get())),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& value) {
            static_cast<Rect*>(e)->SetLocalOpacity(value ? 1.0f : 0.0f);
          },
          base::Unretained(content_frame))));

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    auto element = std::make_unique<VectorIconButton>(
        base::RepeatingCallback<void()>(), spec.icon, audio_delegate_);
    element->SetName(spec.name);
    element->SetDrawPhase(kPhaseForeground);
    element->SetSize(kIndicatorHeightDMM, kIndicatorHeightDMM);
    element->SetIconScaleFactor(kIndicatorIconScaleFactor);
    element->set_hover_offset(0.0f);
    element->SetSounds(Sounds(), audio_delegate_);
    element->AddBinding(std::make_unique<Binding<bool>>(
        VR_BIND_LAMBDA(
            [](Model* model, bool CapturingStateModel::*signal) {
              return model->active_capturing.*signal ||
                     model->background_capturing.*signal;
            },
            base::Unretained(model_), spec.signal),
        VR_BIND_LAMBDA(
            [](UiElement* view, const bool& value) {
              view->SetVisible(value);
              view->set_requires_layout(value);
            },
            base::Unretained(element.get()))));
    element->AddBinding(std::make_unique<Binding<std::pair<bool, bool>>>(
        VR_BIND_LAMBDA(
            [](UiElement* parent, UiElement* child) {
              return std::make_pair(parent->FirstLaidOutChild() == child,
                                    parent->LastLaidOutChild() == child);
            },
            base::Unretained(indicator_layout.get()),
            base::Unretained(element.get())),
        VR_BIND_LAMBDA(
            [](UiElement* view, const std::pair<bool, bool>& value) {
              CornerRadii radii;
              radii.upper_left = value.first ? kIndicatorCornerRadiusDMM : 0.0f;
              radii.lower_left = radii.upper_left;
              radii.upper_right =
                  value.second ? kIndicatorCornerRadiusDMM : 0.0f;
              radii.lower_right = radii.upper_right;
              view->SetCornerRadii(radii);
            },
            base::Unretained(element.get()))));
    VR_BIND_BUTTON_COLORS(model_, element.get(), &ColorScheme::indicator,
                          &Button::SetButtonColors);

    auto tooltip = Create<Oval>(kNone, kPhaseForeground);
    VR_BIND_COLOR(model_, tooltip.get(),
                  &ColorScheme::webvr_permission_background, &Rect::SetColor);
    tooltip->set_bounds_contain_children(true);
    tooltip->set_contributes_to_parent_bounds(false);
    tooltip->set_padding(kIndicatorXPaddingDMM, kIndicatorYPaddingDMM,
                         kIndicatorXPaddingDMM, kIndicatorYPaddingDMM);
    tooltip->set_y_anchoring(BOTTOM);
    tooltip->set_y_centering(TOP);
    tooltip->SetVisible(false);
    tooltip->SetTranslate(0, kIndicatorOffsetDMM, 0);
    tooltip->set_owner_name_for_test(element->name());
    tooltip->SetTransitionedProperties({OPACITY});
    tooltip->SetType(kTypeTooltip);
    tooltip->AddBinding(VR_BIND_FUNC(bool, Button, element.get(),
                                     model->hovered(), UiElement, tooltip.get(),
                                     SetVisible));

    auto text_element =
        Create<Text>(kNone, kPhaseForeground, kWebVrPermissionFontHeight);
    text_element->SetLayoutMode(kSingleLine);
    text_element->SetColor(SK_ColorWHITE);
    text_element->set_owner_name_for_test(element->name());
    text_element->SetType(kTypeLabel);
    BindIndicatorText(model_, text_element.get(), spec);
    VR_BIND_COLOR(model_, text_element.get(),
                  &ColorScheme::webvr_permission_foreground, &Text::SetColor);

    tooltip->AddChild(std::move(text_element));
    element->AddChild(std::move(tooltip));
    indicator_layout->AddChild(std::move(element));
  }
  backplane->AddChild(std::move(indicator_layout));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(backplane));
}

void UiSceneCreator::CreateContentQuad() {
  // Place an invisible but hittable plane behind the content quad, to keep the
  // reticle roughly planar with the content if near content.
  auto hit_plane = Create<InvisibleHitTarget>(kBackplane, kPhaseBackplanes);
  hit_plane->SetSize(kBackplaneSize, kSceneHeight);
  hit_plane->set_contributes_to_parent_bounds(false);

  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(hit_plane));

  auto resizer = Create<Resizer>(kContentResizer, kPhaseNone);
  resizer->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                   model->reposition_window_enabled(), Resizer,
                                   resizer.get(), SetEnabled));
  resizer->AddBinding(
      VR_BIND_FUNC(gfx::PointF, Model, model_,
                   model->primary_controller().touchpad_touch_position, Resizer,
                   resizer.get(), set_touch_position));
  resizer->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, model->primary_controller().touching_touchpad,
      Resizer, resizer.get(), SetTouchingTouchpad));

  auto main_content = std::make_unique<ContentElement>(
      content_input_delegate_,
      base::BindRepeating(&UiBrowserInterface::OnContentScreenBoundsChanged,
                          base::Unretained(browser_)));
  EventHandlers event_handlers;
  event_handlers.focus_change = base::BindRepeating(
      [](Model* model, ContentElement* e, ContentInputDelegate* delegate,
         bool focused) {
        if (!focused) {
          model->set_web_input_text_field_info(EditedText());
          delegate->ClearTextInputState();
        }
        e->UpdateInput(model->web_input_text_field_info);
      },
      model_, base::Unretained(main_content.get()),
      base::Unretained(content_input_delegate_));
  main_content->set_event_handlers(event_handlers);
  main_content->SetName(kContentQuad);
  main_content->set_hit_testable(true);
  main_content->SetDrawPhase(kPhaseForeground);
  main_content->SetSize(kContentWidth, kContentHeight);
  main_content->SetCornerRadius(kContentCornerRadius);
  main_content->SetTransitionedProperties({BOUNDS});
  main_content->SetTextInputDelegate(text_input_delegate_);
  main_content->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](Model* m) { return m->editing_web_input; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](ContentElement* e, const bool& v) {
            if (v) {
              e->RequestFocus();
            } else {
              e->RequestUnfocus();
            }
          },
          base::Unretained(main_content.get()))));
  main_content->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              main_content.get(),
              view->SetSize(value ? kFullscreenWidth : kContentWidth,
                            value ? kFullscreenHeight : kContentHeight)));
  main_content->AddBinding(
      VR_BIND_FUNC(gfx::Transform, Model, model_, model->projection_matrix,
                   ContentElement, main_content.get(), SetProjectionMatrix));
  main_content->AddBinding(
      VR_BIND_FUNC(unsigned int, Model, model_, model->content_texture_id,
                   ContentElement, main_content.get(), SetTextureId));
  main_content->AddBinding(
      VR_BIND_FUNC(GlTextureLocation, Model, model_, model->content_location,
                   ContentElement, main_content.get(), SetTextureLocation));
  main_content->AddBinding(VR_BIND_FUNC(
      unsigned int, Model, model_, model->content_overlay_texture_id,
      ContentElement, main_content.get(), SetOverlayTextureId));
  main_content->AddBinding(VR_BIND_FUNC(
      GlTextureLocation, Model, model_, model->content_overlay_location,
      ContentElement, main_content.get(), SetOverlayTextureLocation));
  main_content->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, !model->content_overlay_texture_non_empty,
      ContentElement, main_content.get(), SetOverlayTextureEmpty));
  main_content->AddBinding(std::make_unique<Binding<base::TimeTicks>>(
      VR_BIND_LAMBDA([](Model* m) { return m->web_input_text_field_touched; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA([](ContentElement* e, EditedText const* v,
                        base::TimeTicks const&) { e->UpdateInput(*v); },
                     base::Unretained(main_content.get()),
                     base::Unretained(&model_->web_input_text_field_info))));

  auto indicator_bg = Create<Rect>(kLoadingIndicator, kPhaseForeground);
  indicator_bg->set_contributes_to_parent_bounds(false);
  indicator_bg->set_bounds_contain_children(true);
  indicator_bg->set_y_anchoring(BOTTOM);
  indicator_bg->set_y_centering(BOTTOM);
  indicator_bg->SetTranslate(0, kLoadingIndicatorYOffset, 0);
  indicator_bg->SetCornerRadii(
      {0, 0, kContentCornerRadius, kContentCornerRadius});
  indicator_bg->SetTransitionedProperties({OPACITY});
  VR_BIND_VISIBILITY(indicator_bg, model->loading);
  VR_BIND_COLOR(model_, indicator_bg.get(),
                &ColorScheme::loading_indicator_background, &Rect::SetColor);

  auto indicator_fg =
      Create<Rect>(kLoadingIndicatorForeground, kPhaseForeground);
  indicator_fg->SetCornerRadii(
      {0, 0, kContentCornerRadius, kContentCornerRadius});
  // Start at content width; size is later updated in a content callback.
  indicator_fg->SetSize(kContentWidth, kLoadingIndicatorHeight);
  VR_BIND_COLOR(model_, indicator_fg.get(),
                &ColorScheme::loading_indicator_foreground, &Rect::SetColor);
  indicator_fg->AddBinding(std::make_unique<Binding<float>>(
      VR_BIND_LAMBDA([](Model* m) { return m->load_progress; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Rect* r, const float& progress) {
            r->SetClipRect({0, 0, progress, 1});
          },
          base::Unretained(indicator_fg.get()))));

  // Have content explicitly resize the loading indicator as it changes, in lieu
  // of adding a UI framework capability.
  main_content->set_on_size_changed_callback(base::BindRepeating(
      [](Rect* rect, const gfx::SizeF& size) {
        rect->SetSize(size.width(), kLoadingIndicatorHeight);
      },
      base::Unretained(indicator_fg.get())));

  indicator_bg->AddChild(std::move(indicator_fg));
  main_content->AddChild(std::move(indicator_bg));

  auto frame = Create<Rect>(kContentFrame, kPhaseForeground);
  frame->set_hit_testable(true);
  frame->set_bounds_contain_children(true);
  frame->set_padding(kRepositionFrameEdgePadding, kRepositionFrameTopPadding,
                     kRepositionFrameEdgePadding, kRepositionFrameEdgePadding);
  frame->SetCornerRadius(kContentCornerRadius);
  frame->set_bounds_contain_padding(false);
  frame->SetLocalOpacity(0.0f);
  frame->SetTransitionedProperties({LOCAL_OPACITY});
  frame->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kRepositionFrameTransitionDurationMs));
  VR_BIND_COLOR(model_, frame.get(), &ColorScheme::content_reposition_frame,
                &Rect::SetColor);

  auto plane =
      Create<InvisibleHitTarget>(kContentFrameHitPlane, kPhaseForeground);
  plane->set_bounds_contain_children(true);
  plane->set_bounds_contain_padding(false);
  plane->SetCornerRadius(kContentCornerRadius);
  plane->set_cursor_type(kCursorReposition);
  Sounds sounds;
  sounds.button_up = kSoundButtonClick;
  plane->SetSounds(sounds, audio_delegate_);
  plane->set_padding(0, kRepositionFrameHitPlaneTopPadding, 0, 0);
  plane->set_event_handlers(CreateRepositioningHandlers(model_, scene_));
  plane->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                 model->reposition_window_permitted(),
                                 UiElement, plane.get(), set_hit_testable));

  resizer->AddChild(std::move(main_content));
  plane->AddChild(std::move(resizer));
  frame->AddChild(std::move(plane));
  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(frame));

  // Limit reticle distance to a sphere based on maximum content distance.
  scene_->set_background_distance(kFullscreenDistance *
                                  kBackgroundDistanceMultiplier);
}

void UiSceneCreator::CreateExternalPromptNotifcationOverlay() {
#if !defined(OS_ANDROID)
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
  text1->SetLayoutMode(kMultiLineFixedWidth);
  text1->SetAlignment(kTextAlignmentLeft);
  text1->SetFieldWidth(kPromptTextWidth);
  VR_BIND_COLOR(model_, text1.get(), &ColorScheme::modal_prompt_foreground,
                &Text::SetColor);
  Text* line1_text = text1.get();

  auto text2 = Create<Text>(kNone, phase, kPromptFontSize);
  text2->SetType(kTypePromptText);
  text2->SetLayoutMode(kMultiLineFixedWidth);
  text2->SetAlignment(kTextAlignmentLeft);
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
  prompt_window->set_hit_testable(false);
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
  VR_BIND_VISIBILITY(scaler, model->web_vr_enabled() &&
                                 (model->web_vr.external_prompt_notification !=
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
                NOTREACHED();
            }

            text_element->SetText(l10n_util::GetStringUTF16(message_id));
            icon_element->SetIcon(icon);
          },
          base::Unretained(line1_text), base::Unretained(vector_icon))));

  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(scaler));
#endif  // !defined(OS_ANDROID)
}

void UiSceneCreator::CreateWebVrSubtree() {
  CreateWebVrOverlayElements();
  CreateWebVrTimeoutScreen();
  CreateExternalPromptNotifcationOverlay();

  // This is needed to for accepting permissions in WebVR mode.
  auto hosted_ui_root =
      CreateHostedUi(model_, browser_, kWebVrHostedUi, kWebVrHostedUiContent,
                     kTimeoutScreenDisatance);
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(hosted_ui_root));

  // Note, this cannot be a descendant of the viewport aware root, otherwise it
  // will fade out when the viewport aware elements reposition.
  auto bg = std::make_unique<FullScreenRect>();
  bg->SetName(kWebVrBackground);
  bg->SetDrawPhase(kPhaseBackground);
  bg->SetVisible(false);
  bg->SetColor(model_->color_scheme().web_vr_background);
  bg->SetTransitionedProperties({OPACITY});
  VR_BIND_VISIBILITY(bg, model->web_vr_enabled() &&
                             (!model->web_vr.IsImmersiveWebXrVisible() ||
                              model->web_vr.showing_hosted_ui ||
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
          [](Model* model, UiElement* timeout_screen) {
            return model->web_vr_enabled() &&
                   (model->web_vr.showing_hosted_ui ||
                    timeout_screen->GetTargetOpacity() != 0.f);
          },
          base::Unretained(model_),
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

  auto spinner = std::make_unique<Spinner>(512);
  spinner->SetName(kWebVrTimeoutSpinner);
  spinner->SetDrawPhase(kPhaseForeground);
  spinner->SetTransitionedProperties({OPACITY});
  spinner->SetVisible(false);
  spinner->SetSize(kTimeoutSpinnerSizeDMM, kTimeoutSpinnerSizeDMM);
  spinner->SetTranslate(0, kTimeoutSpinnerVerticalOffsetDMM, 0);
  spinner->SetColor(model_->color_scheme().web_vr_timeout_spinner);
  spinner->set_hit_testable(true);
  VR_BIND_VISIBILITY(spinner, model->web_vr.state == kWebVrTimeoutImminent);

  auto timeout_message = Create<Rect>(kWebVrTimeoutMessage, kPhaseForeground);
  timeout_message->SetVisible(false);
  timeout_message->set_hit_testable(true);
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
  timeout_icon->set_hit_testable(true);
  timeout_icon->SetSize(kTimeoutMessageIconWidthDMM,
                        kTimeoutMessageIconHeightDMM);

  auto timeout_text = Create<Text>(kWebVrTimeoutMessageText, kPhaseForeground,
                                   kTimeoutMessageTextFontHeightDMM);
  timeout_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_TIMEOUT_MESSAGE));
  timeout_text->SetColor(
      model_->color_scheme().web_vr_timeout_message_foreground);
  timeout_text->SetAlignment(kTextAlignmentLeft);
  timeout_text->SetFieldWidth(kTimeoutMessageTextWidthDMM);
  timeout_text->set_hit_testable(true);

#if !defined(OS_WIN)
  auto button_scaler =
      std::make_unique<ScaledDepthAdjuster>(kTimeoutButtonDepthOffset);

  auto button =
      Create<DiscButton>(kWebVrTimeoutMessageButton, kPhaseForeground,
                         base::BindRepeating(&UiBrowserInterface::ExitPresent,
                                             base::Unretained(browser_)),
                         vector_icons::kCloseRoundedIcon, audio_delegate_);
  button->SetVisible(false);
  button->SetTranslate(0, -kTimeoutMessageTextWidthDMM, 0);
  button->SetRotate(1, 0, 0, kTimeoutButtonRotationRad);
  button->SetTransitionedProperties({OPACITY});
  button->SetSize(kWebVrTimeoutMessageButtonDiameterDMM,
                  kWebVrTimeoutMessageButtonDiameterDMM);
  VR_BIND_VISIBILITY(button, model->web_vr.state == kWebVrTimedOut);
  VR_BIND_BUTTON_COLORS(model_, button.get(), &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  auto timeout_button_text =
      Create<Text>(kWebVrTimeoutMessageButtonText, kPhaseForeground,
                   kTimeoutMessageTextFontHeightDMM);

  // Disc-style button text is not uppercase. See https://crbug.com/787654.
  timeout_button_text->SetText(
      l10n_util::GetStringUTF16(IDS_VR_WEB_VR_EXIT_BUTTON_LABEL));
  timeout_button_text->SetColor(model_->color_scheme().web_vr_timeout_spinner);
  timeout_button_text->SetFieldWidth(kTimeoutButtonTextWidthDMM);
  timeout_button_text->set_contributes_to_parent_bounds(false);
  timeout_button_text->set_y_anchoring(BOTTOM);
  timeout_button_text->SetTranslate(0, -kTimeoutButtonTextVerticalOffsetDMM, 0);
  timeout_button_text->set_hit_testable(true);

  button->AddChild(std::move(timeout_button_text));
  button_scaler->AddChild(std::move(button));
#endif  // !defined(OS_WIN)

  timeout_layout->AddChild(std::move(timeout_icon));
  timeout_layout->AddChild(std::move(timeout_text));
  timeout_message->AddChild(std::move(timeout_layout));

#if !defined(OS_WIN)
  timeout_message->AddChild(std::move(button_scaler));
#endif  // !defined(OS_WIN)

  scaler->AddChild(std::move(timeout_message));
  scaler->AddChild(std::move(spinner));
  scene_->AddUiElement(kWebVrViewportAwareRoot, std::move(scaler));
}

void UiSceneCreator::CreateBackground() {
  // Textured background.
  auto background =
      Create<Background>(k2dBrowsingTexturedBackground, kPhaseBackground);
  background->SetVisible(true);
  VR_BIND_VISIBILITY(background, model->background_loaded);
  background->AddBinding(
      VR_BIND_FUNC(float, Model, model_, model->color_scheme().normal_factor,
                   Background, background.get(), SetNormalFactor));
  background->AddBinding(
      VR_BIND_FUNC(float, Model, model_, model->color_scheme().incognito_factor,
                   Background, background.get(), SetIncognitoFactor));
  background->AddBinding(VR_BIND_FUNC(
      float, Model, model_, model->color_scheme().fullscreen_factor, Background,
      background.get(), SetFullscreenFactor));
  scene_->AddUiElement(k2dBrowsingBackground, std::move(background));

  auto stars = Create<Stars>(kStars, kPhaseBackground);
  stars->SetRotate(1, 0, 0, base::kPiFloat * 0.5);
  scene_->AddUiElement(k2dBrowsingTexturedBackground, std::move(stars));

  auto grid = CreateGrid(model_, kNone);
  ApplyFloorTransform(grid.get());
  VR_BIND_COLOR(model_, grid.get(), &ColorScheme::floor_grid,
                &Grid::SetGridColor);
  grid->SetOpacity(kGridOpacity);
  scene_->AddUiElement(k2dBrowsingTexturedBackground, std::move(grid));

  // Fallback background.
  auto element = Create<UiElement>(k2dBrowsingDefaultBackground, kPhaseNone);
  VR_BIND_VISIBILITY(element, !model->background_loaded);
  scene_->AddUiElement(k2dBrowsingBackground, std::move(element));

  auto solid_background =
      Create<FullScreenRect>(kSolidBackground, kPhaseBackground);
  VR_BIND_COLOR(model_, solid_background.get(), &ColorScheme::world_background,
                &Rect::SetColor);
  VR_BIND_VISIBILITY(solid_background, model->browsing_enabled());
  scene_->AddUiElement(k2dBrowsingDefaultBackground,
                       std::move(solid_background));

  auto grid_fallback = CreateGrid(model_, kNone);
  grid_fallback->set_owner_name_for_test(kFloor);
  VR_BIND_COLOR(model_, grid_fallback.get(), &ColorScheme::floor_grid,
                &Grid::SetGridColor);
  auto floor = Create<Rect>(kFloor, kPhaseBackground);
  ApplyFloorTransform(floor.get());
  VR_BIND_COLOR(model_, floor.get(), &ColorScheme::floor,
                &Rect::SetCenterColor);
  VR_BIND_COLOR(model_, floor.get(), &ColorScheme::world_background,
                &Rect::SetEdgeColor);
  floor->AddChild(std::move(grid_fallback));
  scene_->AddUiElement(k2dBrowsingDefaultBackground, std::move(floor));

  // Ceiling.
  auto ceiling = Create<Rect>(kCeiling, kPhaseBackground);
  ceiling->set_hit_testable(true);
  ceiling->set_focusable(false);
  ceiling->SetSize(kSceneSize, kSceneSize);
  ceiling->SetTranslate(0.0, kSceneHeight / 2, 0.0);
  ceiling->SetRotate(1, 0, 0, base::kPiFloat / 2);
  VR_BIND_COLOR(model_, ceiling.get(), &ColorScheme::ceiling,
                &Rect::SetCenterColor);
  VR_BIND_COLOR(model_, ceiling.get(), &ColorScheme::world_background,
                &Rect::SetEdgeColor);
  scene_->AddUiElement(k2dBrowsingDefaultBackground, std::move(ceiling));
}

void UiSceneCreator::CreateViewportAwareRoot() {
  auto element = std::make_unique<ViewportAwareRoot>();
  element->SetName(kWebVrViewportAwareRoot);

  // On Windows, allow the viewport-aware UI to translate as well as rotate, so
  // it remains centered appropriately if the user moves.  Only enabled for
  // OS_WIN, since it conflicts with browser UI that isn't shown on Windows.
#if defined(OS_WIN)
  element->SetRecenterOnRotate(true);
#endif
  scene_->AddUiElement(kWebVrRoot, std::move(element));

  element = std::make_unique<ViewportAwareRoot>();
  element->SetName(k2dBrowsingViewportAwareRoot);
  element->set_contributes_to_parent_bounds(false);
#if defined(OS_WIN)
  element->SetRecenterOnRotate(true);
#endif
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(element));
}

void UiSceneCreator::CreateVoiceSearchUiGroup() {
  auto speech_recognition_root = std::make_unique<UiElement>();
  speech_recognition_root->SetName(kSpeechRecognitionRoot);
  speech_recognition_root->SetVisible(false);
  speech_recognition_root->set_contributes_to_parent_bounds(false);
  speech_recognition_root->SetTranslate(0.f, 0.f, -kContentDistance);
  speech_recognition_root->SetTransitionedProperties({OPACITY});
  speech_recognition_root->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(
          kSpeechRecognitionOpacityAnimationDurationMs));
  VR_BIND_VISIBILITY(speech_recognition_root, model->voice_search_active());

  auto inner_circle = std::make_unique<Rect>();
  inner_circle->SetName(kSpeechRecognitionCircle);
  inner_circle->SetDrawPhase(kPhaseForeground);
  inner_circle->SetSize(kCloseButtonDiameter * 2, kCloseButtonDiameter * 2);
  inner_circle->SetCornerRadius(kCloseButtonDiameter);
  VR_BIND_COLOR(model_, inner_circle.get(),
                &ColorScheme::speech_recognition_circle_background,
                &Rect::SetColor);

  auto microphone_icon = std::make_unique<VectorIcon>(512);
  microphone_icon->SetIcon(vector_icons::kMicIcon);
  microphone_icon->SetName(kSpeechRecognitionMicrophoneIcon);
  microphone_icon->SetDrawPhase(kPhaseForeground);
  microphone_icon->SetSize(kCloseButtonDiameter, kCloseButtonDiameter);

  auto speech_result_parent =
      Create<UiElement>(kSpeechRecognitionResult, kPhaseNone);
  speech_result_parent->SetTransitionedProperties({OPACITY});
  speech_result_parent->SetTransitionDuration(base::TimeDelta::FromMilliseconds(
      kSpeechRecognitionOpacityAnimationDurationMs));
  speech_result_parent->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return !m->speech.recognition_result.empty(); },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& v) {
            if (v)
              e->SetVisibleImmediately(true);
            else
              e->SetVisible(false);
          },
          speech_result_parent.get())));
  auto speech_result =
      std::make_unique<Text>(kVoiceSearchRecognitionResultTextHeight);
  speech_result->SetName(kSpeechRecognitionResultText);
  speech_result->SetDrawPhase(kPhaseForeground);
  speech_result->SetTranslate(0.f, kSpeechRecognitionResultTextYOffset, 0.f);
  speech_result->SetFieldWidth(kVoiceSearchRecognitionResultTextWidth);
  speech_result->SetAlignment(kTextAlignmentCenter);
  VR_BIND_COLOR(model_, speech_result.get(), &ColorScheme::prompt_foreground,
                &Text::SetColor);
  speech_result->AddBinding(VR_BIND_FUNC(base::string16, Model, model_,
                                         model->speech.recognition_result, Text,
                                         speech_result.get(), SetText));
  speech_result_parent->AddChild(std::move(speech_result));

  auto hit_target = std::make_unique<InvisibleHitTarget>();
  hit_target->SetName(kSpeechRecognitionResultBackplane);
  hit_target->SetDrawPhase(kPhaseForeground);
  hit_target->SetSize(kBackplaneSize, kBackplaneSize);
  speech_result_parent->AddChild(std::move(hit_target));

  auto speech_recognition_listening = std::make_unique<UiElement>();
  speech_recognition_listening->SetName(kSpeechRecognitionListening);
  VR_BIND_VISIBILITY(speech_recognition_listening,
                     model->speech.recognition_result.empty());

  auto growing_circle = std::make_unique<Throbber>();
  growing_circle->SetName(kSpeechRecognitionListeningGrowingCircle);
  growing_circle->SetDrawPhase(kPhaseForeground);
  growing_circle->SetSize(kCloseButtonDiameter * 2, kCloseButtonDiameter * 2);
  growing_circle->SetCornerRadius(kCloseButtonDiameter);
  VR_BIND_COLOR(model_, growing_circle.get(),
                &ColorScheme::speech_recognition_circle_background,
                &Rect::SetColor);
  growing_circle->AddBinding(
      VR_BIND(int, Model, model_, model->speech.speech_recognition_state,
              Throbber, growing_circle.get(),
              view->SetCircleGrowAnimationEnabled(
                  value == SPEECH_RECOGNITION_IN_SPEECH ||
                  value == SPEECH_RECOGNITION_RECOGNIZING ||
                  value == SPEECH_RECOGNITION_READY)));

  auto close_button = Create<DiscButton>(
      kSpeechRecognitionListeningCloseButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::SetVoiceSearchActive,
                          base::Unretained(browser_), false),
      vector_icons::kCloseRoundedIcon, audio_delegate_);
  close_button->SetSize(kVoiceSearchCloseButtonDiameter,
                        kVoiceSearchCloseButtonDiameter);
  close_button->set_hover_offset(kButtonZOffsetHoverDMM * kContentDistance);
  close_button->SetTranslate(0, -kVoiceSearchCloseButtonYOffset, 0);
  close_button->SetRotate(
      1, 0, 0, atan(-kVoiceSearchCloseButtonYOffset / kContentDistance));
  VR_BIND_BUTTON_COLORS(model_, close_button.get(),
                        &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  speech_recognition_listening->AddChild(std::move(growing_circle));
  speech_recognition_listening->AddChild(std::move(close_button));

  speech_recognition_root->AddChild(std::move(inner_circle));
  speech_recognition_root->AddChild(std::move(microphone_icon));
  speech_recognition_root->AddChild(std::move(speech_recognition_listening));
  speech_recognition_root->AddChild(std::move(speech_result_parent));

  scene_->AddUiElement(k2dBrowsingRepositioner,
                       std::move(speech_recognition_root));
}

void UiSceneCreator::CreateContentRepositioningAffordance() {
  auto content_toggle =
      Create<UiElement>(kContentRepositionVisibilityToggle, kPhaseNone);
  content_toggle->SetTransitionedProperties({OPACITY});
  content_toggle->set_bounds_contain_children(true);
  content_toggle->AddBinding(VR_BIND_FUNC(
      float, Model, model_,
      model->reposition_window_enabled() ? kRepositionContentOpacity : 1.0f,
      UiElement, content_toggle.get(), SetOpacity));
  scene_->AddParentUiElement(k2dBrowsingForeground, std::move(content_toggle));

  auto hit_plane =
      Create<InvisibleHitTarget>(kContentRepositionHitPlane, kPhaseForeground);
  hit_plane->set_contributes_to_parent_bounds(false);
  hit_plane->SetSize(kSceneSize, kSceneSize);
  hit_plane->SetTranslate(0.0f, 0.0f, -kContentDistance);
  hit_plane->set_cursor_type(kCursorReposition);
  Sounds sounds;
  sounds.button_up = kSoundButtonClick;
  hit_plane->SetSounds(sounds, audio_delegate_);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* m) {
        DCHECK(m->reposition_window_enabled());
        m->pop_mode(kModeRepositionWindow);
      },
      base::Unretained(model_));
  hit_plane->set_event_handlers(event_handlers);
  VR_BIND_VISIBILITY(hit_plane, model->reposition_window_enabled());
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(hit_plane));
}

namespace {

bool ControllerVisibility(Model* model) {
#if defined(OS_WIN)
  return model->browsing_enabled() ||
         model->hosted_platform_ui.hosted_ui_enabled;
#else
  return model->browsing_enabled() || model->web_vr.state == kWebVrTimedOut ||
         model->hosted_platform_ui.hosted_ui_enabled;
#endif
}

}  // namespace

void UiSceneCreator::CreateControllers() {
  // Controllers are only visible on Android.
  auto root = std::make_unique<UiElement>();
  root->SetName(kControllerRoot);

  VR_BIND_VISIBILITY(root, ControllerVisibility(model));
  scene_->AddUiElement(kRoot, std::move(root));

  auto group = Create<UiElement>(kNone, kPhaseNone);
  group->SetName(kControllerGroup);

  // Set up the vector binding to manage controllers dynamically.
  ControllerSetBinding::ModelAddedCallback added_callback =
      base::BindRepeating(&OnControllerModelAdded, base::Unretained(scene_),
                          base::Unretained(model_));
  ControllerSetBinding::ModelRemovedCallback removed_callback =
      base::BindRepeating(&OnControllerModelRemoved, base::Unretained(scene_));

  group->AddBinding(std::make_unique<ControllerSetBinding>(
      &model_->controllers, added_callback, removed_callback));

  scene_->AddUiElement(kControllerRoot, std::move(group));

  auto reticle_laser_group = Create<UiElement>(kReticleLaserGroup, kPhaseNone);
  reticle_laser_group->SetTransitionedProperties({OPACITY});
  reticle_laser_group->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kControllerLabelTransitionDurationMs));
  VR_BIND_VISIBILITY(reticle_laser_group, !model->reposition_window_enabled());

  auto laser = std::make_unique<Laser>(model_);
  laser->SetDrawPhase(kPhaseForeground);
  laser->AddBinding(VR_BIND_FUNC(float, Model, model_,
                                 model->primary_controller().opacity, Laser,
                                 laser.get(), SetOpacity));

  auto reticle = std::make_unique<Reticle>(scene_, model_);
  reticle->SetDrawPhase(kPhaseForeground);
  VR_BIND_VISIBILITY(reticle, model->reticle.target_point != gfx::Point3F());

  auto reposition_group = Create<UiElement>(kRepositionCursor, kPhaseNone);
  VR_BIND_VISIBILITY(reposition_group,
                     model->reticle.cursor_type == kCursorReposition);

  auto reposition_bg = Create<Rect>(kNone, kPhaseForeground);
  reposition_bg->set_owner_name_for_test(kRepositionCursor);
  reposition_bg->SetType(kTypeCursorBackground);
  reposition_bg->SetSize(kRepositionCursorBackgroundSize,
                         kRepositionCursorBackgroundSize);
  reposition_bg->SetDrawPhase(kPhaseForeground);
  VR_BIND_COLOR(model_, reposition_bg.get(),
                &ColorScheme::cursor_background_edge, &Rect::SetEdgeColor);
  VR_BIND_COLOR(model_, reposition_bg.get(),
                &ColorScheme::cursor_background_center, &Rect::SetCenterColor);

  auto reposition_icon = std::make_unique<VectorIcon>(128);
  reposition_icon->set_owner_name_for_test(kRepositionCursor);
  reposition_icon->SetType(kTypeCursorForeground);
  reposition_icon->SetIcon(kRepositionIcon);
  reposition_icon->SetDrawPhase(kPhaseForeground);
  reposition_icon->SetSize(kRepositionCursorSize, kRepositionCursorSize);
  VR_BIND_COLOR(model_, reposition_icon.get(), &ColorScheme::cursor_foreground,
                &VectorIcon::SetColor);

  reposition_group->AddChild(std::move(reposition_bg));
  reposition_group->AddChild(std::move(reposition_icon));

  reticle->AddChild(std::move(reposition_group));

  reticle_laser_group->AddChild(std::move(laser));
  reticle_laser_group->AddChild(std::move(reticle));

  scene_->AddUiElement(kControllerGroup, std::move(reticle_laser_group));
}

void UiSceneCreator::CreateKeyboard() {
  auto scaler = std::make_unique<ScaledDepthAdjuster>(kKeyboardDistance);
  scaler->SetName(kKeyboardDmmRoot);

  auto keyboard = std::make_unique<Keyboard>();
  keyboard->SetKeyboardDelegate(keyboard_delegate_);
  keyboard->SetDrawPhase(kPhaseForeground);
  keyboard->SetTranslate(0.0, kKeyboardVerticalOffsetDMM, 0.0);
  keyboard->AddBinding(std::make_unique<Binding<std::pair<bool, gfx::PointF>>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return std::pair<bool, gfx::PointF>(
                m->primary_controller().touching_touchpad,
                m->primary_controller().touchpad_touch_position);
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Keyboard* keyboard, const std::pair<bool, gfx::PointF>& value) {
            keyboard->OnTouchStateUpdated(value.first, value.second);
          },
          base::Unretained(keyboard.get()))));
  keyboard->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA([](Model* m) { return m->editing_web_input; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, const bool& enabled) {
            if (enabled) {
              e->SetTranslate(
                  0.0, kKeyboardVerticalOffsetDMM * kKeyboardWebInputOffset,
                  0.0);
            } else {
              e->SetTranslate(0.0, kKeyboardVerticalOffsetDMM, 0.0);
            }
          },
          base::Unretained(keyboard.get()))));
  VR_BIND_VISIBILITY(keyboard, (model->editing_input ||
                                (model->editing_web_input &&
                                 (model->get_mode() == kModeBrowsing ||
                                  model->get_mode() == kModeFullscreen))));
  scene_->AddPerFrameCallback(base::BindRepeating(
      [](Keyboard* keyboard) { keyboard->AdvanceKeyboardFrameIfNeeded(); },
      base::Unretained(keyboard.get())));
  scaler->AddChild(std::move(keyboard));
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(scaler));
}

void UiSceneCreator::CreateUrlBar() {
  auto positioner = Create<UiElement>(kUrlBarPositioner, kPhaseNone);
  positioner->set_y_anchoring(BOTTOM);
  positioner->SetTranslate(0, kUrlBarRelativeOffset, 0);
  positioner->set_contributes_to_parent_bounds(false);
  scene_->AddUiElement(k2dBrowsingForeground, std::move(positioner));

  auto scaler = std::make_unique<ScaledDepthAdjuster>(kUrlBarDistance);
  scaler->SetName(kUrlBarDmmRoot);
  scaler->set_contributes_to_parent_bounds(false);
  scene_->AddUiElement(kUrlBarPositioner, std::move(scaler));

  auto url_bar = Create<Rect>(kUrlBar, kPhaseForeground);
  url_bar->SetRotate(1, 0, 0, kUrlBarRotationRad);
  url_bar->set_bounds_contain_children(true);
  url_bar->SetCornerRadius(kUrlBarHeightDMM / 2);
  url_bar->SetTransitionedProperties({FOREGROUND_COLOR, BACKGROUND_COLOR});
  VR_BIND_VISIBILITY(url_bar, !model->fullscreen_enabled());
  VR_BIND_COLOR(model_, url_bar.get(), &ColorScheme::url_bar_background,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarDmmRoot, std::move(url_bar));

  auto layout =
      Create<LinearLayout>(kUrlBarLayout, kPhaseNone, LinearLayout::kRight);
  layout->set_bounds_contain_children(true);
  scene_->AddUiElement(kUrlBar, std::move(layout));

  auto back_button = Create<VectorIconButton>(
      kUrlBarBackButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::NavigateBack,
                          base::Unretained(browser_)),
      vector_icons::kBackArrowIcon, audio_delegate_);
  back_button->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  back_button->SetCornerRadii(
      {kUrlBarHeightDMM / 2, 0, kUrlBarHeightDMM / 2, 0});
  back_button->set_hover_offset(0);
  back_button->SetIconScaleFactor(kUrlBarButtonIconSizeDMM / kUrlBarHeightDMM);
  back_button->SetIconTranslation(kUrlBarEndButtonIconOffsetDMM, 0);
  back_button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                       model->can_navigate_back, Button,
                                       back_button.get(), SetEnabled));
  VR_BIND_BUTTON_COLORS(model_, back_button.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(back_button));

  auto separator = Create<Rect>(kUrlBarLeftSeparator, kPhaseForeground);
  separator->set_hit_testable(true);
  separator->SetSize(kUrlBarSeparatorWidthDMM, kUrlBarHeightDMM);
  VR_BIND_COLOR(model_, separator.get(), &ColorScheme::url_bar_separator,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarLayout, std::move(separator));

  auto url_click_callback = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->needs_keyboard_update) {
          browser->OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate);
        } else {
          model->push_mode(kModeEditingOmnibox);
        }
      },
      base::Unretained(model_), base::Unretained(browser_));

  auto origin_region = Create<Button>(kUrlBarOriginRegion, kPhaseForeground,
                                      url_click_callback, audio_delegate_);
  origin_region->set_hit_testable(true);
  origin_region->set_bounds_contain_children(true);
  origin_region->set_hover_offset(0);
  VR_BIND_BUTTON_COLORS(model_, origin_region.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(origin_region));

  // This layout contains the page info icon and URL.
  auto origin_layout = Create<LinearLayout>(kUrlBarOriginLayout, kPhaseNone,
                                            LinearLayout::kRight);
  VR_BIND_VISIBILITY(origin_layout,
                     model->location_bar_state.should_display_url);

  scene_->AddUiElement(kUrlBarOriginRegion, std::move(origin_layout));

  // This layout contains hint-text items, shown when there's no origin.
  auto hint_layout =
      Create<LinearLayout>(kUrlBarHintLayout, kPhaseNone, LinearLayout::kRight);
  VR_BIND_VISIBILITY(hint_layout,
                     !model->location_bar_state.should_display_url);
  scene_->AddUiElement(kUrlBarOriginRegion, std::move(hint_layout));

  auto security_button_region =
      Create<Rect>(kUrlBarSecurityButtonRegion, kPhaseNone);
  security_button_region->SetType(kTypeSpacer);
  security_button_region->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(security_button_region));

  auto security_button = Create<VectorIconButton>(
      kUrlBarSecurityButton, kPhaseForeground,
      base::BindRepeating(&UiBrowserInterface::ShowPageInfo,
                          base::Unretained(browser_)),
      gfx::kNoneIcon, audio_delegate_);
  security_button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
  security_button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
  security_button->SetCornerRadius(kUrlBarItemCornerRadiusDMM);
  security_button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
  VR_BIND_BUTTON_COLORS(model_, security_button.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  security_button->AddBinding(std::make_unique<Binding<const gfx::VectorIcon*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->location_bar_state.vector_icon; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](VectorIconButton* e, const gfx::VectorIcon* const& icon) {
            if (icon != nullptr) {
              e->SetIcon(*icon);
            }
          },
          security_button.get())));
  security_button->AddBinding(std::make_unique<Binding<ButtonColors>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            ButtonColors colors = m->color_scheme().url_bar_button;
            if (m->location_bar_state.security_level ==
                security_state::SecurityLevel::DANGEROUS) {
              colors.foreground = m->color_scheme().url_bar_dangerous_icon;
            }
            return colors;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](VectorIconButton* e, const ButtonColors& colors) {
            e->SetButtonColors(colors);
          },
          base::Unretained(security_button.get()))));
  scene_->AddUiElement(kUrlBarSecurityButtonRegion, std::move(security_button));

  auto url_text =
      Create<UrlText>(kUrlBarUrlText, kPhaseForeground, kUrlBarFontHeightDMM);
  url_text->SetFieldWidth(kUrlBarUrlWidthDMM);
  url_text->AddBinding(VR_BIND_FUNC(GURL, Model, model_,
                                    model->location_bar_state.gurl, UrlText,
                                    url_text.get(), SetUrl));
  VR_BIND_COLOR(model_, url_text.get(), &ColorScheme::url_text_emphasized,
                &UrlText::SetEmphasizedColor);
  VR_BIND_COLOR(model_, url_text.get(), &ColorScheme::url_text_deemphasized,
                &UrlText::SetDeemphasizedColor);
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(url_text));

  auto right_margin = Create<Rect>(kNone, kPhaseNone);
  right_margin->SetType(kTypeSpacer);
  right_margin->SetSize(kUrlBarOriginRightMarginDMM, 0);
  scene_->AddUiElement(kUrlBarOriginLayout, std::move(right_margin));

  auto hint_text_spacer = Create<Rect>(kNone, kPhaseNone);
  hint_text_spacer->SetType(kTypeSpacer);
  hint_text_spacer->SetSize(kUrlBarOriginContentOffsetDMM, kUrlBarHeightDMM);
  scene_->AddUiElement(kUrlBarHintLayout, std::move(hint_text_spacer));

  auto hint_text =
      Create<Text>(kUrlBarHintText, kPhaseForeground, kUrlBarFontHeightDMM);
  hint_text->SetFieldWidth(kUrlBarOriginRegionWidthDMM -
                           kUrlBarOriginContentOffsetDMM);
  hint_text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
  hint_text->SetAlignment(kTextAlignmentLeft);
  hint_text->SetText(l10n_util::GetStringUTF16(IDS_SEARCH_OR_TYPE_WEB_ADDRESS));
  VR_BIND_COLOR(model_, hint_text.get(), &ColorScheme::url_bar_hint_text,
                &Text::SetColor);
  scene_->AddUiElement(kUrlBarHintLayout, std::move(hint_text));

  separator = Create<Rect>(kUrlBarRightSeparator, kPhaseForeground);
  separator->set_hit_testable(true);
  separator->SetSize(kUrlBarSeparatorWidthDMM, kUrlBarHeightDMM);
  VR_BIND_COLOR(model_, separator.get(), &ColorScheme::url_bar_separator,
                &Rect::SetColor);
  scene_->AddUiElement(kUrlBarLayout, std::move(separator));

  auto overflow_button = Create<VectorIconButton>(
      kUrlBarOverflowButton, kPhaseForeground,
      base::BindRepeating(
          [](Model* model) { model->overflow_menu_enabled = true; },
          base::Unretained(model_)),
      kMoreVertIcon, audio_delegate_);
  overflow_button->SetSize(kUrlBarEndButtonWidthDMM, kUrlBarHeightDMM);
  overflow_button->SetCornerRadii(
      {0, kUrlBarHeightDMM / 2, 0, kUrlBarHeightDMM / 2});
  overflow_button->set_hover_offset(0);
  overflow_button->SetIconScaleFactor(kUrlBarButtonIconSizeDMM /
                                      kUrlBarHeightDMM);
  overflow_button->SetIconTranslation(-kUrlBarEndButtonIconOffsetDMM, 0);
  VR_BIND_BUTTON_COLORS(model_, overflow_button.get(),
                        &ColorScheme::url_bar_button, &Button::SetButtonColors);
  scene_->AddUiElement(kUrlBarLayout, std::move(overflow_button));
}

void UiSceneCreator::CreateOverflowMenu() {
  auto overflow_backplane =
      Create<InvisibleHitTarget>(kOverflowMenuBackplane, kPhaseForeground);
  EventHandlers event_handlers;
  event_handlers.button_up = base::BindRepeating(
      [](Model* model) { model->overflow_menu_enabled = false; },
      base::Unretained(model_));
  overflow_backplane->set_event_handlers(event_handlers);
  overflow_backplane->SetSize(kBackplaneSize, kBackplaneSize);
  overflow_backplane->set_contributes_to_parent_bounds(false);
  overflow_backplane->set_y_anchoring(TOP);
  overflow_backplane->SetRotate(1, 0, 0, -kUrlBarRotationRad);
  VR_BIND_VISIBILITY(overflow_backplane, model->overflow_menu_enabled);

  auto overflow_menu = Create<Rect>(kOverflowMenu, kPhaseForeground);
  overflow_menu->set_hit_testable(true);
  overflow_menu->set_y_centering(BOTTOM);
  overflow_menu->set_bounds_contain_children(true);
  overflow_menu->set_contributes_to_parent_bounds(false);
  overflow_menu->SetTranslate(0, kOverflowMenuOffset, 0);
  overflow_menu->SetCornerRadius(kUrlBarItemCornerRadiusDMM);
  VR_BIND_COLOR(model_, overflow_menu.get(), &ColorScheme::omnibox_background,
                &Rect::SetColor);

  auto overflow_outer_layout =
      Create<LinearLayout>(kNone, kPhaseNone, LinearLayout::kUp);

  auto button_region = Create<UiElement>(kNone, kPhaseNone);
  button_region->set_bounds_contain_children(true);
  button_region->set_y_anchoring(BOTTOM);
  button_region->set_y_centering(BOTTOM);
  button_region->set_contributes_to_parent_bounds(false);

  auto button_region_bg = Create<Rect>(kNone, kPhaseForeground);
  button_region_bg->SetSize(kOverflowMenuMinimumWidth,
                            kOverflowButtonRegionHeight);
  button_region_bg->SetCornerRadii(
      {0.0f, 0.0f, kUrlBarItemCornerRadiusDMM, kUrlBarItemCornerRadiusDMM});
  button_region_bg->SetOpacity(kOverflowButtonRegionOpacity);
  VR_BIND_COLOR(model_, button_region_bg.get(),
                &ColorScheme::omnibox_background, &Rect::SetColor);
  button_region->AddChild(std::move(button_region_bg));

  // The forward and refresh buttons are anchored to the bottom corners of the
  // reserved space. In the future, when we have more buttons, they may instead
  // be placed in a linear layout (locked to one side).
  std::vector<
      std::tuple<UiElementName, LayoutAlignment, const gfx::VectorIcon&>>
      menu_buttons = {
          {kOverflowMenuForwardButton, LEFT, vector_icons::kForwardArrowIcon},
          {kOverflowMenuReloadButton, RIGHT, vector_icons::kReloadIcon},
      };
  for (auto& item : menu_buttons) {
    auto button = Create<VectorIconButton>(std::get<0>(item), kPhaseForeground,
                                           base::DoNothing(), std::get<2>(item),
                                           audio_delegate_);
    button->SetType(kTypeOverflowMenuButton);
    button->SetDrawPhase(kPhaseForeground);
    button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
    button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
    button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
    button->SetCornerRadius(kUrlBarItemCornerRadiusDMM);
    button->set_requires_layout(false);
    button->set_contributes_to_parent_bounds(false);
    button->set_x_anchoring(std::get<1>(item));
    button->set_x_centering(std::get<1>(item));
    button->set_y_anchoring(BOTTOM);
    button->set_y_centering(BOTTOM);
    button->SetTranslate(
        kOverflowButtonXPadding * (std::get<1>(item) == RIGHT ? -1 : 1),
        kOverflowMenuYPadding, 0);
    VR_BIND_BUTTON_COLORS(model_, button.get(), &ColorScheme::url_bar_button,
                          &Button::SetButtonColors);

    switch (std::get<0>(item)) {
      case kOverflowMenuForwardButton:
        button->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->NavigateForward();
            },
            base::Unretained(model_), base::Unretained(browser_)));
        button->AddBinding(VR_BIND_FUNC(bool, Model, model_,
                                        model->can_navigate_forward, Button,
                                        button.get(), SetEnabled));
        break;
      case kOverflowMenuReloadButton:
        button->set_click_handler(base::BindRepeating(
            [](Model* model, UiBrowserInterface* browser) {
              model->overflow_menu_enabled = false;
              browser->ReloadTab();
            },
            base::Unretained(model_), base::Unretained(browser_)));
        break;
      default:
        break;
    }

    button_region->AddChild(std::move(button));
  }

  int new_incognito_tab_res_id = IDS_VR_MENU_NEW_INCOGNITO_TAB;
  int close_incognito_tabs_res_id = IDS_VR_MENU_CLOSE_INCOGNITO_TABS;

  struct MenuItem {
    UiElementName name;
    int string_id;
    base::RepeatingCallback<void(UiBrowserInterface*)> action;
    base::RepeatingCallback<bool(Model*)> visibility;
  };
  std::vector<MenuItem> menu_items = {
      {
          kOverflowMenuNewIncognitoTabItem,
          new_incognito_tab_res_id,
          base::BindRepeating(
              [](UiBrowserInterface* browser) { browser->OpenNewTab(true); }),
          base::BindRepeating([](Model* m) { return !m->incognito; }),
      },
      {kOverflowMenuCloseAllIncognitoTabsItem, close_incognito_tabs_res_id,
       base::BindRepeating([](UiBrowserInterface* browser) {
         browser->CloseAllIncognitoTabs();
       }),
       base::BindRepeating([](Model* m) { return m->incognito_tabs_open; })},
      {kOverflowMenuPreferencesItem, IDS_VR_MENU_PREFERENCES,
       base::BindRepeating(
           [](UiBrowserInterface* browser) { browser->OpenSettings(); }),
       base::BindRepeating([](Model* m) { return m->standalone_vr_device; })},
  };

  auto overflow_menu_scroll = Create<ScrollableElement>(
      kNone, kPhaseNone, ScrollableElement::kVertical);
  overflow_menu_scroll->set_max_span(kOverflowMenuMaxSpan);
  overflow_menu_scroll->SetScrollAnchoring(TOP);
  auto overflow_menu_layout = Create<LinearLayout>(
      kOverflowMenuLayout, kPhaseNone, LinearLayout::kDown);

  for (auto& item : menu_items) {
    auto layout = std::make_unique<LinearLayout>(LinearLayout::kRight);
    layout->SetType(kTypeOverflowMenuItem);
    layout->SetDrawPhase(kPhaseNone);

    auto text =
        Create<Text>(kNone, kPhaseForeground, kSuggestionContentTextHeightDMM);
    text->SetDrawPhase(kPhaseForeground);
    text->SetText(l10n_util::GetStringUTF16(item.string_id));
    text->SetLayoutMode(TextLayoutMode::kSingleLineFixedWidth);
    text->SetFieldWidth(kOverflowMenuMinimumWidth -
                        2 * kOverflowMenuItemXPadding);
    text->SetAlignment(kTextAlignmentLeft);
    VR_BIND_COLOR(model_, text.get(), &ColorScheme::menu_text, &Text::SetColor);
    layout->AddChild(std::move(text));

    auto spacer = Create<Rect>(kNone, kPhaseNone);
    spacer->SetType(kTypeSpacer);
    spacer->SetSize(0, kOverflowMenuItemHeight);
    layout->AddChild(std::move(spacer));

    auto background = Create<Button>(item.name, kPhaseForeground,
                                     base::DoNothing(), audio_delegate_);
    background->set_hit_testable(true);
    background->set_bounds_contain_children(true);
    background->set_hover_offset(0);
    background->set_padding(kOverflowMenuItemXPadding, 0);
    VR_BIND_BUTTON_COLORS(model_, background.get(),
                          &ColorScheme::url_bar_button,
                          &Button::SetButtonColors);
    background->AddChild(std::move(layout));
    base::RepeatingClosure callback =
        base::BindRepeating(item.action, base::Unretained(browser_));
    background->set_click_handler(base::BindRepeating(
        [](Model* model, const base::RepeatingClosure& callback) {
          model->overflow_menu_enabled = false;
          callback.Run();
        },
        base::Unretained(model_), callback));
    background->AddBinding(std::make_unique<Binding<bool>>(
        VR_BIND_LAMBDA(item.visibility, base::Unretained(model_)),
        VR_BIND_LAMBDA(
            [](UiElement* e, const bool& value) { e->SetVisible(value); },
            base::Unretained(background.get()))));

    overflow_menu_layout->AddChild(std::move(background));
  }

  // The item that reserves space in the menu layout for the buttons.
  auto button_spacer =
      CreateSpacer(kOverflowMenuMinimumWidth, kOverflowButtonRegionHeight);
  overflow_menu_layout->AddChild(std::move(button_spacer));

  overflow_menu_scroll->AddScrollingChild(std::move(overflow_menu_layout));
  overflow_outer_layout->AddChild(std::move(overflow_menu_scroll));

  auto top_cap = Create<Rect>(kNone, kPhaseNone);
  top_cap->SetType(kTypeSpacer);
  top_cap->SetSize(kOverflowMenuMinimumWidth, kOverflowMenuYPadding);
  overflow_outer_layout->AddChild(std::move(top_cap));
  overflow_menu->AddChild(std::move(overflow_outer_layout));

  overflow_menu->AddChild(std::move(button_region));

  overflow_backplane->AddChild(std::move(overflow_menu));
  scene_->AddUiElement(kUrlBarOverflowButton, std::move(overflow_backplane));
}

void UiSceneCreator::CreateOmnibox() {
  auto scaler =
      Create<ScaledDepthAdjuster>(kOmniboxDmmRoot, kPhaseNone, kUrlBarDistance);

  auto omnibox_root = Create<UiElement>(kOmniboxRoot, kPhaseNone);
  omnibox_root->SetVisible(false);
  omnibox_root->SetTransitionedProperties({OPACITY});
  omnibox_root->SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kOmniboxTransitionMs));
  VR_BIND_VISIBILITY(omnibox_root, model->get_mode() == kModeEditingOmnibox);

  auto omnibox_outer_layout =
      Create<LinearLayout>(kOmniboxOuterLayout, kPhaseNone, LinearLayout::kUp);

  auto omnibox_suggestion_divider = Create<Rect>(kNone, kPhaseForeground);
  omnibox_suggestion_divider->SetType(kTypeSpacer);
  omnibox_suggestion_divider->SetSize(kOmniboxWidthDMM, kSuggestionGapDMM);
  VR_BIND_COLOR(model_, omnibox_suggestion_divider.get(),
                &ColorScheme::url_bar_separator, &Rect::SetColor);

  auto omnibox_text_field = Create<OmniboxTextField>(
      kOmniboxTextField, kPhaseNone, kOmniboxTextHeightDMM,
      base::BindRepeating(
          [](Model* model, const EditedText& text_input_info) {
            model->set_omnibox_text_field_info(text_input_info);
          },
          base::Unretained(model_)),
      base::BindRepeating(
          [](UiBrowserInterface* browser, const AutocompleteRequest& request) {
            browser->StartAutocomplete(request);
          },
          base::Unretained(browser_)),
      base::BindRepeating(
          [](UiBrowserInterface* browser) { browser->StopAutocomplete(); },
          base::Unretained(browser_)));
  omnibox_text_field->SetTextInputDelegate(text_input_delegate_);
  omnibox_text_field->set_hit_testable(false);
  omnibox_text_field->SetHintText(
      l10n_util::GetStringUTF16(IDS_SEARCH_OR_TYPE_WEB_ADDRESS));
  // TODO(crbug.com/834308): Refactor this element to be resized by a
  // fixed-width layout, rather than adjusting based on other elements.
  omnibox_text_field->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(&Model::voice_search_available, base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, const bool& mic_button_visible) {
            float width = kOmniboxWidthDMM - 2 * kOmniboxTextMarginDMM;
            if (mic_button_visible) {
              width -= kOmniboxTextFieldIconButtonSizeDMM +
                       kOmniboxTextFieldRightMargin;
            }
            e->SetSize(width, 0);
          },
          base::Unretained(omnibox_text_field.get()))));

  EventHandlers event_handlers;
  event_handlers.focus_change = base::BindRepeating(
      [](Model* model, TextInput* text_input, bool focused) {
        if (focused) {
          model->editing_input = true;
          text_input->UpdateInput(model->omnibox_text_field_info);
        } else {
          model->editing_input = false;
          model->pop_mode(kModeEditingOmnibox);
        }
      },
      base::Unretained(model_), base::Unretained(omnibox_text_field.get()));
  omnibox_text_field->set_event_handlers(event_handlers);

  omnibox_text_field->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, model->has_mode_in_stack(kModeEditingOmnibox),
      OmniboxTextField, omnibox_text_field.get(), SetEnabled));
  omnibox_text_field->AddBinding(std::make_unique<Binding<base::TimeTicks>>(
      VR_BIND_LAMBDA(
          [](Model* model) { return model->omnibox_text_field_touched; },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](OmniboxTextField* e, const EditedText* value,
                        base::TimeTicks const&) { e->UpdateInput(*value); },
                     base::Unretained(omnibox_text_field.get()),
                     base::Unretained(&model_->omnibox_text_field_info))));
  omnibox_text_field->set_input_committed_callback(base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser, Ui* ui,
         const EditedText& text) {
        if (text.current.text == base::UTF8ToUTF16(kCrashVrBrowserUrl))
          CrashIntentionally();
        if (!model->omnibox_suggestions.empty()) {
          browser->Navigate(model->omnibox_suggestions.front().destination,
                            NavigationMethod::kOmniboxUrlEntry);
          ui->OnUiRequestedNavigation();
        }
      },
      base::Unretained(model_), base::Unretained(browser_),
      base::Unretained(ui_)));
  omnibox_text_field->AddBinding(std::make_unique<Binding<Autocompletion>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            if (!m->omnibox_suggestions.empty()) {
              return m->omnibox_suggestions.front().autocompletion;
            } else {
              return Autocompletion();
            }
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA([](OmniboxTextField* e,
                        const Autocompletion& v) { e->SetAutocompletion(v); },
                     base::Unretained(omnibox_text_field.get()))));
  omnibox_text_field->AddBinding(std::make_unique<Binding<bool>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            return m->omnibox_editing_enabled() &&
                   m->active_modal_prompt_type == kModalPromptTypeNone;
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, Model* m, const bool& v) {
            if (v) {
              e->RequestFocus();
            } else {
              e->RequestUnfocus();
            }
          },
          base::Unretained(omnibox_text_field.get()),
          base::Unretained(model_))));
  omnibox_text_field->AddBinding(VR_BIND_FUNC(
      bool, Model, model_, model->supports_selection, OmniboxTextField,
      omnibox_text_field.get(), set_allow_inline_autocomplete));

  VR_BIND_COLOR(model_, omnibox_text_field.get(), &ColorScheme::url_bar_text,
                &TextInput::SetTextColor);
  VR_BIND_COLOR(model_, omnibox_text_field.get(),
                &ColorScheme::url_bar_hint_text, &TextInput::SetHintColor);
  omnibox_text_field->AddBinding(std::make_unique<Binding<TextSelectionColors>>(
      VR_BIND_LAMBDA(
          [](Model* m) { return m->color_scheme().omnibox_text_selection; },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TextInput* e, const TextSelectionColors& colors) {
            e->SetSelectionColors(colors);
          },
          base::Unretained(omnibox_text_field.get()))));

  auto mic_button = Create<VectorIconButton>(
      kOmniboxVoiceSearchButton, kPhaseForeground,
      base::BindRepeating(
          [](UiBrowserInterface* b, Ui* ui) { b->SetVoiceSearchActive(true); },
          base::Unretained(browser_), base::Unretained(ui_)),
      vector_icons::kMicIcon, audio_delegate_);
  mic_button->SetSize(kUrlBarButtonSizeDMM, kUrlBarButtonSizeDMM);
  mic_button->SetIconScaleFactor(kUrlBarButtonIconScaleFactor);
  mic_button->set_hover_offset(kUrlBarButtonHoverOffsetDMM);
  mic_button->SetCornerRadius(kUrlBarItemCornerRadiusDMM);
  VR_BIND_VISIBILITY(mic_button, model->voice_search_available());
  VR_BIND_BUTTON_COLORS(model_, mic_button.get(), &ColorScheme::url_bar_button,
                        &Button::SetButtonColors);

  auto mic_button_spacer =
      CreateSpacer(kOmniboxMicIconRightMarginDMM, kOmniboxHeightDMM);
  VR_BIND_VISIBILITY(mic_button_spacer, model->voice_search_available());

  auto text_field_layout = Create<LinearLayout>(
      kOmniboxTextFieldLayout, kPhaseNone, LinearLayout::kRight);
  text_field_layout->AddChild(
      CreateSpacer(kOmniboxTextMarginDMM, kOmniboxHeightDMM));
  text_field_layout->AddChild(std::move(omnibox_text_field));
  text_field_layout->AddChild(
      CreateSpacer(kOmniboxTextMarginDMM, kOmniboxHeightDMM));
  text_field_layout->AddChild(std::move(mic_button));
  text_field_layout->AddChild(std::move(mic_button_spacer));

  // Set up the vector binding to manage suggestions dynamically.
  SuggestionSetBinding::ModelAddedCallback added_callback = base::BindRepeating(
      &OnSuggestionModelAdded, base::Unretained(scene_),
      base::Unretained(browser_), base::Unretained(ui_),
      base::Unretained(model_), base::Unretained(audio_delegate_));
  SuggestionSetBinding::ModelRemovedCallback removed_callback =
      base::BindRepeating(&OnSuggestionModelRemoved, base::Unretained(scene_));

  auto suggestions_layout =
      Create<LinearLayout>(kOmniboxSuggestions, kPhaseNone, LinearLayout::kUp);
  suggestions_layout->AddBinding(std::make_unique<SuggestionSetBinding>(
      &model_->omnibox_suggestions, added_callback, removed_callback));

  auto button_scaler = Create<ScaledDepthAdjuster>(
      kNone, kPhaseNone, kOmniboxCloseButtonDepthOffset);

  auto close_button = Create<DiscButton>(
      kOmniboxCloseButton, kPhaseForeground,
      base::BindRepeating(
          [](Model* model) { model->pop_mode(kModeEditingOmnibox); },
          base::Unretained(model_)),
      vector_icons::kBackArrowIcon, audio_delegate_);
  close_button->SetSize(kOmniboxCloseButtonDiameterDMM,
                        kOmniboxCloseButtonDiameterDMM);
  close_button->SetTranslate(0, kOmniboxCloseButtonVerticalOffsetDMM, 0);
  close_button->SetRotate(1, 0, 0, atan(kOmniboxCloseButtonVerticalOffsetDMM));
  close_button->set_hover_offset(kButtonZOffsetHoverDMM);
  VR_BIND_BUTTON_COLORS(model_, close_button.get(),
                        &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  auto suggestions_outer_layout = Create<LinearLayout>(
      kOmniboxSuggestionsOuterLayout, kPhaseNone, LinearLayout::kUp);
  VR_BIND_VISIBILITY(suggestions_outer_layout,
                     !model->omnibox_suggestions.empty());
  suggestions_outer_layout->AddChild(std::move(omnibox_suggestion_divider));
  suggestions_outer_layout->AddChild(
      CreateSpacer(kOmniboxWidthDMM, kSuggestionVerticalPaddingDMM));
  suggestions_outer_layout->AddChild(std::move(suggestions_layout));
  suggestions_outer_layout->AddChild(
      CreateSpacer(kOmniboxWidthDMM, kSuggestionVerticalPaddingDMM));

  omnibox_outer_layout->AddChild(std::move(text_field_layout));
  omnibox_outer_layout->AddChild(std::move(suggestions_outer_layout));

  // Rounded-corner background of all omnibox and suggestion elements.
  auto omnibox_background = Create<Rect>(kOmniboxBackground, kPhaseForeground);
  omnibox_background->set_bounds_contain_children(true);
  omnibox_background->set_hit_testable(true);
  omnibox_background->set_y_centering(BOTTOM);
  omnibox_background->set_contributes_to_parent_bounds(false);
  omnibox_background->set_focusable(false);
  omnibox_background->SetCornerRadius(kOmniboxCornerRadiusDMM);
  omnibox_background->SetTranslate(
      0, kOmniboxVerticalOffsetDMM - 0.5 * kOmniboxHeightDMM, 0);
  VR_BIND_COLOR(model_, omnibox_background.get(),
                &ColorScheme::omnibox_background, &Rect::SetColor);
  omnibox_background->AddChild(std::move(omnibox_outer_layout));

  button_scaler->AddChild(std::move(close_button));

  omnibox_root->AddChild(std::move(omnibox_background));
  omnibox_root->AddChild(std::move(button_scaler));

  scaler->AddChild(std::move(omnibox_root));

  UiElement* parent = scene_->GetUiElementByName(k2dBrowsingRepositioner);
  parent->AddChild(std::move(scaler));

  // This binding must run whether or not the omnibox is visible.
  parent->AddBinding(std::make_unique<Binding<std::pair<bool, base::string16>>>(
      VR_BIND_LAMBDA(
          [](Model* m) {
            bool editing_omnibox = m->has_mode_in_stack(kModeEditingOmnibox);
            base::string16 url_text =
                FormatUrlForVr(m->location_bar_state.gurl, nullptr);
            return std::make_pair(editing_omnibox, url_text);
          },
          base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Model* m, const std::pair<bool, base::string16>& value) {
            if (value.first /* editing_omnibox */) {
              EditedText omnibox_text = m->omnibox_text_field_info;
              omnibox_text.current =
                  TextInputInfo(value.second, 0, value.second.size());
              m->set_omnibox_text_field_info(std::move(omnibox_text));
            } else {
              m->set_omnibox_text_field_info(EditedText());
            }
          },
          base::Unretained(model_))));
}

void UiSceneCreator::CreateCloseButton() {
  base::RepeatingCallback<void()> click_handler = base::BindRepeating(
      [](Model* model, UiBrowserInterface* browser) {
        if (model->fullscreen_enabled()) {
          browser->ExitFullscreen();
        }
      },
      base::Unretained(model_), base::Unretained(browser_));
  std::unique_ptr<DiscButton> element =
      Create<DiscButton>(kCloseButton, kPhaseForeground, click_handler,
                         vector_icons::kCloseRoundedIcon, audio_delegate_);
  element->set_contributes_to_parent_bounds(false);
  element->SetSize(kCloseButtonDiameter, kCloseButtonDiameter);
  element->set_hover_offset(kButtonZOffsetHoverDMM * kCloseButtonDistance);
  element->set_y_anchoring(BOTTOM);
  element->SetTranslate(0, kCloseButtonRelativeOffset, -kCloseButtonDistance);
  VR_BIND_BUTTON_COLORS(model_, element.get(), &ColorScheme::disc_button_colors,
                        &DiscButton::SetButtonColors);

  // Close button is a special control element that needs to be hidden when
  // in WebVR, but it needs to be visible when in cct or fullscreen.
  VR_BIND_VISIBILITY(element, model->fullscreen_enabled());
  element->AddBinding(
      VR_BIND(bool, Model, model_, model->fullscreen_enabled(), UiElement,
              element.get(),
              view->SetTranslate(0, kCloseButtonRelativeOffset,
                                 value ? -kCloseButtonFullscreenDistance
                                       : -kCloseButtonDistance)));
  element->AddBinding(VR_BIND(
      bool, Model, model_, model->fullscreen_enabled(), UiElement,
      element.get(),
      view->SetRotate(
          1, 0, 0,
          atan(value ? kCloseButtonFullscreenVerticalOffset /
                           kCloseButtonFullscreenDistance
                     : kCloseButtonVerticalOffset / kCloseButtonDistance))));
  element->AddBinding(VR_BIND(
      bool, Model, model_, model->fullscreen_enabled(), UiElement,
      element.get(),
      view->SetSize(
          value ? kCloseButtonFullscreenDiameter : kCloseButtonDiameter,
          value ? kCloseButtonFullscreenDiameter : kCloseButtonDiameter)));

  scene_->AddUiElement(k2dBrowsingForeground, std::move(element));
}

void UiSceneCreator::CreatePrompts() {
  auto prompt = CreatePrompt(model_);
  prompt->SetName(kExitPrompt);
  VR_BIND_VISIBILITY(prompt,
                     model->active_modal_prompt_type != kModalPromptTypeNone);
  prompt->AddBinding(std::make_unique<Binding<ModalPromptType>>(
      VR_BIND_LAMBDA([](Model* m) { return m->active_modal_prompt_type; },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](UiElement* e, Model* model, UiBrowserInterface* browser,
             const ModalPromptType& type) {
            if (type == kModalPromptTypeNone)
              return;

            int message_id = 0;
            const gfx::VectorIcon* icon = nullptr;
            int primary_button_text_id = 0;
            int secondary_button_text_id = 0;
            ExitVrPromptChoice primary_choice = CHOICE_EXIT;
            ExitVrPromptChoice secondary_choice = CHOICE_STAY;
            UiUnsupportedMode reason = GetReasonForPrompt(type);

            switch (type) {
              case kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission:
                message_id = IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_DESCRIPTION;
                icon = &vector_icons::kMicIcon;
                primary_button_text_id =
                    IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_CONTINUE_BUTTON;
                secondary_button_text_id =
                    IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_ABORT_BUTTON;
                break;
              case kModalPromptTypeUpdateKeyboard:
                message_id = IDS_VR_UPDATE_KEYBOARD_PROMPT;
                icon = &vector_icons::kInfoOutlineIcon;
                primary_button_text_id =
                    IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_CONTINUE_BUTTON;
                secondary_button_text_id =
                    IDS_VR_SHELL_AUDIO_PERMISSION_PROMPT_ABORT_BUTTON;
                break;
              case kModalPromptTypeExitVRForSiteInfo:
                message_id = IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION_SITE_INFO;
                icon = &vector_icons::kInfoOutlineIcon;
                primary_button_text_id =
                    IDS_VR_SHELL_EXIT_PROMPT_EXIT_VR_BUTTON;
                secondary_button_text_id = IDS_VR_BUTTON_BACK;
                break;
              case kModalPromptTypeExitVRForCertificateInfo:
              case kModalPromptTypeExitVRForConnectionSecurityInfo:
              case kModalPromptTypeGenericUnsupportedFeature:
                message_id = IDS_VR_SHELL_EXIT_PROMPT_DESCRIPTION;
                icon = &vector_icons::kInfoOutlineIcon;
                primary_button_text_id =
                    IDS_VR_SHELL_EXIT_PROMPT_EXIT_VR_BUTTON;
                secondary_button_text_id = IDS_VR_BUTTON_BACK;
                break;
              case kNumModalPromptTypes:
              case kModalPromptTypeNone:
                NOTREACHED();
                break;
            }
            Text* text_element =
                static_cast<Text*>(e->GetDescendantByType(kTypePromptText));
            text_element->SetText(l10n_util::GetStringUTF16(message_id));
            VectorIcon* icon_element = static_cast<VectorIcon*>(
                e->GetDescendantByType(kTypePromptIcon));
            icon_element->SetIcon(icon);
            TextButton* primary_button = static_cast<TextButton*>(
                e->GetDescendantByType(kTypePromptPrimaryButton));
            // TODO(crbug.com/787654): Uppercasing should be conditional.
            primary_button->SetText(base::i18n::ToUpper(
                l10n_util::GetStringUTF16(primary_button_text_id)));
            primary_button->set_click_handler(
                CreatePromptCallback(reason, primary_choice, model, browser));
            TextButton* secondary_button = static_cast<TextButton*>(
                e->GetDescendantByType(kTypePromptSecondaryButton));
            // TODO(crbug.com/787654): Uppercasing should be conditional.
            secondary_button->SetText(base::i18n::ToUpper(
                l10n_util::GetStringUTF16(secondary_button_text_id)));
            secondary_button->set_click_handler(
                CreatePromptCallback(reason, secondary_choice, model, browser));
            InvisibleHitTarget* backplane = static_cast<InvisibleHitTarget*>(
                e->GetDescendantByType(kTypePromptBackplane));
            EventHandlers event_handlers;
            event_handlers.button_up =
                CreatePromptCallback(reason, CHOICE_NONE, model, browser);
            backplane->set_event_handlers(event_handlers);
          },
          base::Unretained(prompt.get()), base::Unretained(model_),
          base::Unretained(browser_))));
  scene_->AddUiElement(k2dBrowsingRepositioner, std::move(prompt));
}

void UiSceneCreator::CreateWebVrOverlayElements() {
  // Create transient WebVR elements.
  auto indicators = Create<LinearLayout>(kWebVrIndicatorLayout, kPhaseNone,
                                         LinearLayout::kDown);
  indicators->SetTranslate(0, 0, kWebVrPermissionDepth);
  indicators->set_margin(kWebVrPermissionOuterMargin);

  DrawPhase phase = kPhaseOverlayForeground;

  IndicatorSpec app_button_spec = {kNone,
                                   kWebVrExclusiveScreenToast,
                                   kRemoveCircleOutlineIcon,
                                   IDS_PRESS_APP_TO_EXIT,
                                   0,
                                   0,
                                   nullptr,
                                   false};
  indicators->AddChild(
      CreateWebVrIndicator(model_, browser_, app_button_spec, phase));

  auto specs = GetIndicatorSpecs();
  for (const auto& spec : specs) {
    indicators->AddChild(CreateWebVrIndicator(model_, browser_, spec, phase));
  }

  auto parent = CreateTransientParent(kWebVrIndicatorTransience,
                                      GetIndicatorsTimeout(), true);

#if defined(OS_WIN)
  parent->AddBinding(
      std::make_unique<
          Binding<std::tuple<bool, CapturingStateModel, CapturingStateModel>>>(
          VR_BIND_LAMBDA(
              [](Model* model) {
                return std::tuple<bool, CapturingStateModel,
                                  CapturingStateModel>(
                    model->web_vr_enabled() &&
                        model->web_vr.IsImmersiveWebXrVisible() &&
                        model->web_vr.has_received_permissions,
                    model->active_capturing, model->potential_capturing);
              },
              base::Unretained(model_)),
          VR_BIND_LAMBDA(BindIndicatorTranscienceForWin,
                         base::Unretained(parent.get()),
                         base::Unretained(model_), base::Unretained(scene_))));
#else
  parent->AddBinding(std::make_unique<Binding<std::tuple<bool, bool, bool>>>(
      VR_BIND_LAMBDA(
          [](Model* model) {
            return std::tuple<bool, bool, bool>(
                model->web_vr_enabled() &&
                    model->web_vr.IsImmersiveWebXrVisible() &&
                    model->web_vr.has_received_permissions,
                model->menu_button_long_pressed,
                model->web_vr.showing_hosted_ui);
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

void UiSceneCreator::CreateToasts() {
  auto platform_toast = CreateTextToast(
      kPlatformToastTransientParent, kPlatformToast, model_, base::string16());
  platform_toast->set_contributes_to_parent_bounds(false);
  platform_toast->set_y_anchoring(BOTTOM);
  platform_toast->set_y_centering(TOP);
  platform_toast->SetTranslate(0, kPlatformToastVerticalOffset,
                               kIndicatorDistanceOffset);
  platform_toast->AddBinding(std::make_unique<Binding<const PlatformToast*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->platform_toast.get(); },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](TransientElement* t, const PlatformToast* const& value) {
            t->SetVisible(value);
            if (value) {
              t->RefreshVisible();
            }
          },
          base::Unretained(platform_toast.get()))));

  Text* text_element =
      static_cast<Text*>(platform_toast->GetDescendantByType(kTypeToastText));
  DCHECK(text_element);
  text_element->AddBinding(std::make_unique<Binding<const PlatformToast*>>(
      VR_BIND_LAMBDA([](Model* m) { return m->platform_toast.get(); },
                     base::Unretained(model_)),
      VR_BIND_LAMBDA(
          [](Text* t, const PlatformToast* const& value) {
            if (value) {
              t->SetText(value->text);
            }
          },
          base::Unretained(text_element))));

  scene_->AddUiElement(k2dBrowsingContentGroup, std::move(platform_toast));
}

}  // namespace vr
