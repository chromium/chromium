// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iomanip>
#include <queue>
#include <sstream>
#include <string>
#include <utility>

#include "chrome/browser/vr/ui.h"

#include "base/bind.h"
#include "base/numerics/math_constants.h"
#include "base/numerics/ranges.h"
#include "base/strings/string16.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/elements/content_element.h"
#include "chrome/browser/vr/elements/keyboard.h"
#include "chrome/browser/vr/elements/text_input.h"
#include "chrome/browser/vr/keyboard_delegate.h"
#include "chrome/browser/vr/keyboard_delegate_for_testing.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/model/omnibox_suggestions.h"
#include "chrome/browser/vr/model/platform_toast.h"
#include "chrome/browser/vr/model/sound_id.h"
#include "chrome/browser/vr/platform_input_handler.h"
#include "chrome/browser/vr/platform_ui_input_delegate.h"
#include "chrome/browser/vr/skia_surface_provider_factory.h"
#include "chrome/browser/vr/speech_recognizer.h"
#include "chrome/browser/vr/ui_browser_interface.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "chrome/browser/vr/ui_input_manager_for_testing.h"
#include "chrome/browser/vr/ui_renderer.h"
#include "chrome/browser/vr/ui_scene.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/ui_scene_creator.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace vr {

namespace {

constexpr float kMargin = 1.f * base::kPiFloat / 180;

UiElementName UserFriendlyElementNameToUiElementName(
    UserFriendlyElementName name) {
  switch (name) {
    case UserFriendlyElementName::kUrl:
      return kUrlBarOriginRegion;
    case UserFriendlyElementName::kBackButton:
      return kUrlBarBackButton;
    case UserFriendlyElementName::kForwardButton:
      return kOverflowMenuForwardButton;
    case UserFriendlyElementName::kReloadButton:
      return kOverflowMenuReloadButton;
    case UserFriendlyElementName::kOverflowMenu:
      return kUrlBarOverflowButton;
    case UserFriendlyElementName::kPageInfoButton:
      return kUrlBarSecurityButton;
    case UserFriendlyElementName::kBrowsingDialog:
      return k2dBrowsingHostedUiContent;
    case UserFriendlyElementName::kContentQuad:
      return kContentQuad;
    case UserFriendlyElementName::kNewIncognitoTab:
      return kOverflowMenuNewIncognitoTabItem;
    case UserFriendlyElementName::kCloseIncognitoTabs:
      return kOverflowMenuCloseAllIncognitoTabsItem;
    case UserFriendlyElementName::kExitPrompt:
      return kExitPrompt;
    case UserFriendlyElementName::kSuggestionBox:
      return kOmniboxSuggestions;
    case UserFriendlyElementName::kOmniboxTextField:
      return kOmniboxTextField;
    case UserFriendlyElementName::kOmniboxCloseButton:
      return kOmniboxCloseButton;
    case UserFriendlyElementName::kOmniboxVoiceInputButton:
      return kOmniboxVoiceSearchButton;
    case UserFriendlyElementName::kVoiceInputCloseButton:
      return kSpeechRecognitionListeningCloseButton;
    case UserFriendlyElementName::kAppButtonExitToast:
      return kWebVrExclusiveScreenToast;
    case UserFriendlyElementName::kWebXrAudioIndicator:
      return kWebVrAudioCaptureIndicator;
    case UserFriendlyElementName::kWebXrHostedContent:
      return kWebVrHostedUiContent;
    case UserFriendlyElementName::kMicrophonePermissionIndicator:
      return kAudioCaptureIndicator;
    case UserFriendlyElementName::kWebXrExternalPromptNotification:
      return kWebXrExternalPromptNotification;
    case UserFriendlyElementName::kCameraPermissionIndicator:
      return kVideoCaptureIndicator;
    case UserFriendlyElementName::kLocationPermissionIndicator:
      return kLocationAccessIndicator;
    case UserFriendlyElementName::kWebXrLocationPermissionIndicator:
      return kWebVrLocationAccessIndicator;
    case UserFriendlyElementName::kWebXrVideoPermissionIndicator:
      return kWebVrVideoCaptureIndicator;
    default:
      NOTREACHED();
      return kNone;
  }
}

}  // namespace

Ui::Ui(UiBrowserInterface* browser,
       PlatformInputHandler* content_input_forwarder,
       std::unique_ptr<KeyboardDelegate> keyboard_delegate,
       std::unique_ptr<TextInputDelegate> text_input_delegate,
       std::unique_ptr<AudioDelegate> audio_delegate,
       const UiInitialState& ui_initial_state)
    : Ui(browser,
         std::make_unique<ContentInputDelegate>(content_input_forwarder),
         std::move(keyboard_delegate),
         std::move(text_input_delegate),
         std::move(audio_delegate),
         ui_initial_state) {}

Ui::Ui(UiBrowserInterface* browser,
       std::unique_ptr<ContentInputDelegate> content_input_delegate,
       std::unique_ptr<KeyboardDelegate> keyboard_delegate,
       std::unique_ptr<TextInputDelegate> text_input_delegate,
       std::unique_ptr<AudioDelegate> audio_delegate,
       const UiInitialState& ui_initial_state)
    : browser_(browser),
      scene_(std::make_unique<UiScene>()),
      model_(std::make_unique<Model>()),
      content_input_delegate_(std::move(content_input_delegate)),
      input_manager_(std::make_unique<UiInputManager>(scene_.get())),
      keyboard_delegate_(std::move(keyboard_delegate)),
      text_input_delegate_(std::move(text_input_delegate)),
      audio_delegate_(std::move(audio_delegate)) {
  UiInitialState state = ui_initial_state;
  if (text_input_delegate_) {
    text_input_delegate_->SetRequestFocusCallback(
        base::BindRepeating(&Ui::RequestFocus, base::Unretained(this)));
    text_input_delegate_->SetRequestUnfocusCallback(
        base::BindRepeating(&Ui::RequestUnfocus, base::Unretained(this)));
  }
  if (keyboard_delegate_) {
    keyboard_delegate_->SetUiInterface(this);
    state.supports_selection = keyboard_delegate_->SupportsSelection();
  }
  InitializeModel(state);

  UiSceneCreator(browser, scene_.get(), this, content_input_delegate_.get(),
                 keyboard_delegate_.get(), text_input_delegate_.get(),
                 audio_delegate_.get(), model_.get())
      .CreateScene();
}

Ui::~Ui() = default;

base::WeakPtr<BrowserUiInterface> Ui::GetBrowserUiWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

SchedulerUiInterface* Ui::GetSchedulerUiPtr() {
  return this;
}

void Ui::SetWebVrMode(bool enabled) {
  if (enabled) {
    model_->web_vr.has_received_permissions = false;
    model_->web_vr.state = kWebVrAwaitingFirstFrame;
    if (!model_->web_vr_enabled())
      model_->push_mode(kModeWebVr);
  } else {
    model_->web_vr.state = kWebVrNoTimeoutPending;
    if (model_->web_vr_enabled())
      model_->pop_mode();
  }
}

void Ui::SetFullscreen(bool enabled) {
  if (enabled) {
    model_->push_mode(kModeFullscreen);
  } else {
    model_->pop_mode(kModeFullscreen);
  }
}

void Ui::SetLocationBarState(const LocationBarState& state) {
  model_->location_bar_state = state;
}

void Ui::SetIncognito(bool enabled) {
  model_->incognito = enabled;
}

void Ui::SetLoading(bool loading) {
  model_->loading = loading;
}

void Ui::SetLoadProgress(float progress) {
  model_->load_progress = progress;
}

void Ui::SetHistoryButtonsEnabled(bool can_go_back, bool can_go_forward) {
  model_->can_navigate_back = can_go_back;
  model_->can_navigate_forward = can_go_forward;
}

void Ui::SetCapturingState(const CapturingStateModel& active_capturing,
                           const CapturingStateModel& background_capturing,
                           const CapturingStateModel& potential_capturing) {
  model_->active_capturing = active_capturing;
  model_->background_capturing = background_capturing;
  model_->potential_capturing = potential_capturing;
  model_->web_vr.has_received_permissions = true;
}

void Ui::ShowExitVrPrompt(UiUnsupportedMode reason) {
  // Shouldn't request to exit VR when we're already prompting to exit VR.
  CHECK(model_->active_modal_prompt_type == kModalPromptTypeNone);

  switch (reason) {
    case UiUnsupportedMode::kUnhandledCodePoint:
      NOTREACHED();  // This mode does not prompt.
      break;
    case UiUnsupportedMode::kUnhandledPageInfo:
      model_->active_modal_prompt_type = kModalPromptTypeExitVRForSiteInfo;
      break;
    case UiUnsupportedMode::kVoiceSearchNeedsRecordAudioOsPermission:
      model_->active_modal_prompt_type =
          kModalPromptTypeExitVRForVoiceSearchRecordAudioOsPermission;
      break;
    case UiUnsupportedMode::kGenericUnsupportedFeature:
      model_->active_modal_prompt_type =
          kModalPromptTypeGenericUnsupportedFeature;
      break;
    case UiUnsupportedMode::kNeedsKeyboardUpdate:
      model_->active_modal_prompt_type = kModalPromptTypeUpdateKeyboard;
      break;
    case UiUnsupportedMode::kUnhandledCertificateInfo:
      model_->active_modal_prompt_type =
          kModalPromptTypeExitVRForCertificateInfo;
      break;
    case UiUnsupportedMode::kUnhandledConnectionSecurityInfo:
      model_->active_modal_prompt_type =
          kModalPromptTypeExitVRForConnectionSecurityInfo;
      break;
    // kSearchEnginePromo should DOFF directly. It should never try to change
    // the state of UI.
    case UiUnsupportedMode::kSearchEnginePromo:
    case UiUnsupportedMode::kCount:
      NOTREACHED();  // Should never be used as a mode (when |enabled| is true).
      break;
  }

  if (model_->active_modal_prompt_type != kModalPromptTypeNone) {
    model_->push_mode(kModeModalPrompt);
  }
}

void Ui::OnUiRequestedNavigation() {
  model_->pop_mode(kModeEditingOmnibox);
}

void Ui::SetSpeechRecognitionEnabled(bool enabled) {
  if (enabled) {
    model_->speech.recognition_result.clear();
    DCHECK(!model_->has_mode_in_stack(kModeVoiceSearch));
    model_->push_mode(kModeVoiceSearch);
    model_->push_mode(kModeVoiceSearchListening);
  } else {
    model_->pop_mode(kModeVoiceSearchListening);
    if (model_->speech.recognition_result.empty()) {
      OnSpeechRecognitionEnded();
    } else {
      auto sequence = std::make_unique<Sequence>();
      sequence->Add(
          base::BindOnce(&Ui::OnSpeechRecognitionEnded,
                         weak_ptr_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kSpeechRecognitionResultTimeoutMs));
      scene_->AddSequence(std::move(sequence));
    }
  }
}

void Ui::OnSpeechRecognitionEnded() {
  model_->pop_mode(kModeVoiceSearch);
  if (model_->omnibox_editing_enabled() &&
      !model_->speech.recognition_result.empty()) {
    model_->pop_mode(kModeEditingOmnibox);
  }
}

void Ui::SetRecognitionResult(const base::string16& result) {
  model_->speech.recognition_result = result;
}

void Ui::SetHasOrCanRequestRecordAudioPermission(
    bool const has_or_can_request_record_audio) {
  model_->speech.has_or_can_request_record_audio_permission =
      has_or_can_request_record_audio;
}

void Ui::OnSpeechRecognitionStateChanged(int new_state) {
  model_->speech.speech_recognition_state = new_state;
}

void Ui::SetOmniboxSuggestions(std::vector<OmniboxSuggestion> suggestions) {
  model_->omnibox_suggestions = std::move(suggestions);
}

void Ui::ShowSoftInput(bool show) {
  if (model_->needs_keyboard_update) {
    browser_->OnUnsupportedMode(UiUnsupportedMode::kNeedsKeyboardUpdate);
    return;
  }
  model_->editing_web_input = show;
}

void Ui::UpdateWebInputIndices(int selection_start,
                               int selection_end,
                               int composition_start,
                               int composition_end) {
  content_input_delegate_->OnWebInputIndicesChanged(
      selection_start, selection_end, composition_start, composition_end,
      base::BindOnce(
          [](Model* model, const TextInputInfo& new_state) {
            EditedText web_input_text = model->web_input_text_field_info;
            web_input_text.current = new_state;
            model->set_web_input_text_field_info(std::move(web_input_text));
          },
          base::Unretained(model_.get())));
}

void Ui::SetAlertDialogEnabled(bool enabled,
                               PlatformUiInputDelegate* delegate,
                               float width,
                               float height) {
  model_->web_vr.showing_hosted_ui = enabled;
  model_->hosted_platform_ui.hosted_ui_enabled = enabled;
  model_->hosted_platform_ui.delegate = delegate;

  if (!enabled)
    return;
  SetAlertDialogSize(width, height);
}

void Ui::SetContentOverlayAlertDialogEnabled(bool enabled,
                                             PlatformUiInputDelegate* delegate,
                                             float width_percentage,
                                             float height_percentage) {
  model_->web_vr.showing_hosted_ui = enabled;
  model_->hosted_platform_ui.hosted_ui_enabled = enabled;
  SetContentOverlayAlertDialogSize(width_percentage, height_percentage);
  model_->hosted_platform_ui.delegate = delegate;
}

void Ui::SetAlertDialogSize(float width, float height) {
  float scale = std::max(height, width);
  model_->hosted_platform_ui.rect.set_height(height / scale);
  model_->hosted_platform_ui.rect.set_width(width / scale);
}

void Ui::SetContentOverlayAlertDialogSize(float width_percentage,
                                          float height_percentage) {
  model_->hosted_platform_ui.rect.set_height(height_percentage);
  model_->hosted_platform_ui.rect.set_width(width_percentage);
}

void Ui::SetDialogLocation(float x, float y) {
  model_->hosted_platform_ui.rect.set_y(y);
  model_->hosted_platform_ui.rect.set_x(x);
}

void Ui::SetDialogFloating(bool floating) {
  model_->hosted_platform_ui.floating = floating;
}

void Ui::ShowPlatformToast(const base::string16& text) {
  model_->platform_toast = std::make_unique<PlatformToast>(text);
}

void Ui::CancelPlatformToast() {
  model_->platform_toast.reset();
}

void Ui::OnGlInitialized(GlTextureLocation textures_location,
                         unsigned int content_texture_id,
                         unsigned int content_overlay_texture_id,
                         unsigned int platform_ui_texture_id) {
  ui_element_renderer_ = std::make_unique<UiElementRenderer>();
  ui_renderer_ =
      std::make_unique<UiRenderer>(scene_.get(), ui_element_renderer_.get());
  provider_ = SkiaSurfaceProviderFactory::Create();
  scene_->OnGlInitialized(provider_.get());
  model_->content_texture_id = content_texture_id;
  model_->content_overlay_texture_id = content_overlay_texture_id;
  model_->content_location = textures_location;
  model_->content_overlay_location = textures_location;
  model_->hosted_platform_ui.texture_id = platform_ui_texture_id;
}

void Ui::RequestFocus(int element_id) {
  input_manager_->RequestFocus(element_id);
}

void Ui::RequestUnfocus(int element_id) {
  input_manager_->RequestUnfocus(element_id);
}

void Ui::OnInputEdited(const EditedText& info) {
  input_manager_->OnInputEdited(info);
}

void Ui::OnInputCommitted(const EditedText& info) {
  input_manager_->OnInputCommitted(info);
}

void Ui::OnKeyboardHidden() {
  input_manager_->OnKeyboardHidden();
}

void Ui::OnPause() {
  input_manager_->OnPause();
}

void Ui::OnMenuButtonClicked() {
  // Menu button clicks should be a no-op when browsing mode is disabled.
  if (model_->browsing_disabled)
    return;

  if (model_->reposition_window_enabled()) {
    model_->pop_mode(kModeRepositionWindow);
    return;
  }

  if (model_->editing_web_input) {
    ShowSoftInput(false);
    return;
  }

  if (model_->hosted_platform_ui.hosted_ui_enabled) {
    browser_->CloseHostedDialog();
    return;
  }

  // Menu button click exits the WebVR presentation and fullscreen.
  browser_->ExitPresent();
  browser_->ExitFullscreen();

  switch (model_->get_last_opaque_mode()) {
    case kModeVoiceSearch:
      browser_->SetVoiceSearchActive(false);
      break;
    case kModeEditingOmnibox:
      model_->pop_mode(kModeEditingOmnibox);
      break;
    default:
      break;
  }
}

void Ui::OnControllersUpdated(
    const std::vector<ControllerModel>& controller_models,
    const ReticleModel& reticle_model) {
  model_->controllers = controller_models;
  model_->reticle = reticle_model;
  for (auto& controller : model_->controllers) {
    controller.resting_in_viewport =
        input_manager_->ControllerRestingInViewport();
  }
}

void Ui::OnProjMatrixChanged(const gfx::Transform& proj_matrix) {
  model_->projection_matrix = proj_matrix;
}

void Ui::OnWebXrFrameAvailable() {
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrPresenting;
}

void Ui::OnWebXrTimeoutImminent() {
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrTimeoutImminent;
}

void Ui::OnWebXrTimedOut() {
  if (model_->web_vr_enabled())
    model_->web_vr.state = kWebVrTimedOut;
}

void Ui::OnSwapContents(int new_content_id) {
  content_input_delegate_->OnSwapContents(new_content_id);
}

void Ui::OnContentBoundsChanged(int width, int height) {
  content_input_delegate_->SetSize(width, height);
}

void Ui::Dump(bool include_bindings) {
#ifndef NDEBUG
  std::ostringstream os;
  os << std::setprecision(3);
  os << std::endl;
  scene_->root_element().DumpHierarchy(std::vector<size_t>(), &os,
                                       include_bindings);

  std::stringstream ss(os.str());
  std::string line;
  while (std::getline(ss, line, '\n')) {
    LOG(ERROR) << line;
  }
#endif
}

void Ui::OnAssetsLoaded(AssetsLoadStatus status,
                        std::unique_ptr<Assets> assets,
                        const base::Version& component_version) {
  model_->waiting_for_background = false;

  if (status != AssetsLoadStatus::kSuccess) {
    return;
  }

  Background* background = static_cast<Background*>(
      scene_->GetUiElementByName(k2dBrowsingTexturedBackground));
  DCHECK(background);
  background->SetBackgroundImage(std::move(assets->background));
  background->SetGradientImages(std::move(assets->normal_gradient),
                                std::move(assets->incognito_gradient),
                                std::move(assets->fullscreen_gradient));

  ColorScheme::UpdateForComponent(component_version);
  model_->background_loaded = true;

  if (audio_delegate_) {
    std::vector<std::pair<SoundId, std::unique_ptr<std::string>&>> sounds = {
        {kSoundButtonHover, assets->button_hover_sound},
        {kSoundButtonClick, assets->button_click_sound},
        {kSoundBackButtonClick, assets->back_button_click_sound},
        {kSoundInactiveButtonClick, assets->inactive_button_click_sound},
    };
    audio_delegate_->ResetSounds();
    for (auto& sound : sounds) {
      if (sound.second)
        audio_delegate_->RegisterSound(sound.first, std::move(sound.second));
    }
  }
}

void Ui::OnAssetsUnavailable() {
  model_->waiting_for_background = false;
}

void Ui::WaitForAssets() {
  model_->waiting_for_background = true;
}

void Ui::SetRegularTabsOpen(bool open) {
  model_->regular_tabs_open = open;
}

void Ui::SetIncognitoTabsOpen(bool open) {
  model_->incognito_tabs_open = open;
}

void Ui::SetOverlayTextureEmpty(bool empty) {
  model_->content_overlay_texture_non_empty = !empty;
}

void Ui::ReinitializeForTest(const UiInitialState& ui_initial_state) {
  InitializeModel(ui_initial_state);
}

bool Ui::GetElementVisibilityForTesting(UserFriendlyElementName element_name) {
  auto* target_element = scene()->GetUiElementByName(
      UserFriendlyElementNameToUiElementName(element_name));
  DCHECK(target_element) << "Unsupported test element";
  return target_element->IsVisible();
}

void Ui::SetUiInputManagerForTesting(bool enabled) {
  if (enabled) {
    DCHECK(input_manager_for_testing_ == nullptr)
        << "Attempted to set test UiInputManager while already using it";
    input_manager_for_testing_ =
        std::make_unique<UiInputManagerForTesting>(scene_.get());
    input_manager_for_testing_.swap(input_manager_);
  } else {
    DCHECK(input_manager_for_testing_ != nullptr)
        << "Attempted to unset test UiInputManager while not using it";
    input_manager_for_testing_.swap(input_manager_);
    input_manager_for_testing_.reset();
  }
}

void Ui::InitializeModel(const UiInitialState& ui_initial_state) {
  model_->speech.has_or_can_request_record_audio_permission =
      ui_initial_state.has_or_can_request_record_audio_permission;
  model_->ui_modes.clear();
  model_->push_mode(kModeBrowsing);
  if (ui_initial_state.in_web_vr) {
    auto mode = kModeWebVr;
    model_->web_vr.has_received_permissions = false;
    model_->web_vr.state = kWebVrAwaitingFirstFrame;
    model_->push_mode(mode);
  }

  model_->browsing_disabled = ui_initial_state.browsing_disabled;
  model_->waiting_for_background = ui_initial_state.assets_supported;
  model_->supports_selection = ui_initial_state.supports_selection;
  model_->needs_keyboard_update = ui_initial_state.needs_keyboard_update;
  model_->standalone_vr_device = ui_initial_state.is_standalone_vr_device;
  model_->controllers.push_back(ControllerModel());
}

void Ui::AcceptDoffPromptForTesting() {
  DCHECK(model_->active_modal_prompt_type != kModalPromptTypeNone);
  auto* prompt = scene_->GetUiElementByName(kExitPrompt);
  DCHECK(prompt);
  auto* button = prompt->GetDescendantByType(kTypePromptPrimaryButton);
  DCHECK(button);
  button->OnHoverEnter({0.5f, 0.5f}, base::TimeTicks::Now());
  button->OnButtonDown({0.5f, 0.5f}, base::TimeTicks::Now());
  button->OnButtonUp({0.5f, 0.5f}, base::TimeTicks::Now());
  button->OnHoverLeave(base::TimeTicks::Now());
}

gfx::Point3F Ui::GetTargetPointForTesting(UserFriendlyElementName element_name,
                                          const gfx::PointF& position) {
  auto* target_element = scene()->GetUiElementByName(
      UserFriendlyElementNameToUiElementName(element_name));
  DCHECK(target_element) << "Unsupported test element";
  // The position to click is provided for a unit square, so scale it to match
  // the actual element.
  auto scaled_position = ScalePoint(position, target_element->size().width(),
                                    target_element->size().height());
  gfx::Point3F target(scaled_position.x(), scaled_position.y(), 0.0f);
  target_element->ComputeTargetWorldSpaceTransform().TransformPoint(&target);
  // We do hit testing with respect to the eye position (world origin), so we
  // need to project the target point into the background.
  gfx::Vector3dF direction = target - kOrigin;
  direction.GetNormalized(&direction);
  return kOrigin +
         gfx::ScaleVector3d(direction, scene()->background_distance());
}

void Ui::PerformKeyboardInputForTesting(KeyboardTestInput keyboard_input) {
  DCHECK(keyboard_delegate_);
  if (keyboard_input.action == KeyboardTestAction::kRevertToRealKeyboard) {
    if (using_keyboard_delegate_for_testing_) {
      DCHECK(static_cast<KeyboardDelegateForTesting*>(keyboard_delegate_.get())
                 ->IsQueueEmpty())
          << "Attempted to revert to real keyboard with input still queued";
      using_keyboard_delegate_for_testing_ = false;
      keyboard_delegate_for_testing_.swap(keyboard_delegate_);
      static_cast<Keyboard*>(
          scene_->GetUiElementByName(UiElementName::kKeyboard))
          ->SetKeyboardDelegate(keyboard_delegate_.get());
      text_input_delegate_->SetUpdateInputCallback(
          base::BindRepeating(&KeyboardDelegate::UpdateInput,
                              base::Unretained(keyboard_delegate_.get())));
    }
    return;
  }
  if (!using_keyboard_delegate_for_testing_) {
    using_keyboard_delegate_for_testing_ = true;
    if (!keyboard_delegate_for_testing_) {
      keyboard_delegate_for_testing_ =
          std::make_unique<KeyboardDelegateForTesting>();
      keyboard_delegate_for_testing_->SetUiInterface(this);
    }
    keyboard_delegate_for_testing_.swap(keyboard_delegate_);
    static_cast<Keyboard*>(scene_->GetUiElementByName(UiElementName::kKeyboard))
        ->SetKeyboardDelegate(keyboard_delegate_.get());
    text_input_delegate_->SetUpdateInputCallback(
        base::BindRepeating(&KeyboardDelegate::UpdateInput,
                            base::Unretained(keyboard_delegate_.get())));
  }
  if (keyboard_input.action != KeyboardTestAction::kEnableMockedKeyboard) {
    static_cast<KeyboardDelegateForTesting*>(keyboard_delegate_.get())
        ->QueueKeyboardInputForTesting(keyboard_input);
  }
}

void Ui::SetVisibleExternalPromptNotification(
    ExternalPromptNotificationType prompt) {
  model_->web_vr.external_prompt_notification = prompt;
}

ContentElement* Ui::GetContentElement() {
  if (!content_element_) {
    content_element_ =
        static_cast<ContentElement*>(scene()->GetUiElementByName(kContentQuad));
  }
  return content_element_;
}

bool Ui::IsContentVisibleAndOpaque() {
  return GetContentElement()->IsVisibleAndOpaque();
}

void Ui::SetContentUsesQuadLayer(bool uses_quad_layer) {
  return GetContentElement()->SetUsesQuadLayer(uses_quad_layer);
}

gfx::Transform Ui::GetContentWorldSpaceTransform() {
  return GetContentElement()->world_space_transform();
}

bool Ui::OnBeginFrame(base::TimeTicks current_time,
                      const gfx::Transform& head_pose) {
  model_->current_time = current_time;
  return scene_->OnBeginFrame(current_time, head_pose);
}

bool Ui::SceneHasDirtyTextures() const {
  return scene_->HasDirtyTextures();
}

void Ui::UpdateSceneTextures() {
  scene_->UpdateTextures();
}

void Ui::Draw(const vr::RenderInfo& info) {
  ui_renderer_->Draw(info);
}

void Ui::DrawContent(const float (&uv_transform)[16],
                     float xborder,
                     float yborder) {
  if (!model_->content_texture_id || !model_->content_overlay_texture_id)
    return;
  ui_element_renderer_->DrawTextureCopy(model_->content_texture_id,
                                        uv_transform, xborder, yborder);
  if (!GetContentElement()->GetOverlayTextureEmpty()) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    ui_element_renderer_->DrawTextureCopy(model_->content_overlay_texture_id,
                                          uv_transform, xborder, yborder);
  }
}

void Ui::DrawWebXr(int texture_data_handle, const float (&uv_transform)[16]) {
  if (!texture_data_handle)
    return;
  ui_element_renderer_->DrawTextureCopy(texture_data_handle, uv_transform, 0,
                                        0);
}

void Ui::DrawWebVrOverlayForeground(const vr::RenderInfo& info) {
  ui_renderer_->DrawWebVrOverlayForeground(info);
}

bool Ui::HasWebXrOverlayElementsToDraw() {
  return scene_->HasWebXrOverlayElementsToDraw();
}

void Ui::HandleInput(base::TimeTicks current_time,
                     const RenderInfo& render_info,
                     const ControllerModel& controller_model,
                     ReticleModel* reticle_model,
                     InputEventList* input_event_list) {
  HandleMenuButtonEvents(input_event_list);
  input_manager_->HandleInput(current_time, render_info, controller_model,
                              reticle_model, input_event_list);
}

void Ui::HandleMenuButtonEvents(InputEventList* input_event_list) {
  auto it = input_event_list->begin();
  while (it != input_event_list->end()) {
    if (InputEvent::IsMenuButtonEventType((*it)->type())) {
      switch ((*it)->type()) {
        case InputEvent::kMenuButtonClicked:
          // Post a task, rather than calling directly, to avoid modifying UI
          // state in the midst of frame rendering.
          base::ThreadTaskRunnerHandle::Get()->PostTask(
              FROM_HERE,
              base::BindOnce(&Ui::OnMenuButtonClicked, base::Unretained(this)));
          break;
        case InputEvent::kMenuButtonLongPressStart:
          model_->menu_button_long_pressed = true;
          break;
        case InputEvent::kMenuButtonLongPressEnd:
          model_->menu_button_long_pressed = false;
          break;
        default:
          NOTREACHED();
      }
      it = input_event_list->erase(it);
    } else {
      ++it;
    }
  }
}

std::pair<FovRectangle, FovRectangle> Ui::GetMinimalFovForWebXrOverlayElements(
    const gfx::Transform& left_view,
    const FovRectangle& fov_recommended_left,
    const gfx::Transform& right_view,
    const FovRectangle& fov_recommended_right,
    float z_near) {
  auto elements = scene_->GetWebVrOverlayElementsToDraw();
  return {GetMinimalFov(left_view, elements, fov_recommended_left, z_near),
          GetMinimalFov(right_view, elements, fov_recommended_right, z_near)};
}

FovRectangle Ui::GetMinimalFov(const gfx::Transform& view_matrix,
                               const std::vector<const UiElement*>& elements,
                               const FovRectangle& fov_recommended,
                               float z_near) {
  // Calculate boundary of Z near plane in view space.
  float z_near_left =
      -z_near * std::tan(fov_recommended.left * base::kPiFloat / 180);
  float z_near_right =
      z_near * std::tan(fov_recommended.right * base::kPiFloat / 180);
  float z_near_bottom =
      -z_near * std::tan(fov_recommended.bottom * base::kPiFloat / 180);
  float z_near_top =
      z_near * std::tan(fov_recommended.top * base::kPiFloat / 180);

  float left = z_near_right;
  float right = z_near_left;
  float bottom = z_near_top;
  float top = z_near_bottom;

  bool has_visible_element = false;

  for (const auto* element : elements) {
    gfx::Point3F left_bottom{-0.5, -0.5, 0};
    gfx::Point3F left_top{-0.5, 0.5, 0};
    gfx::Point3F right_bottom{0.5, -0.5, 0};
    gfx::Point3F right_top{0.5, 0.5, 0};

    gfx::Transform transform = element->world_space_transform();
    transform.ConcatTransform(view_matrix);

    // Transform to view space.
    transform.TransformPoint(&left_bottom);
    transform.TransformPoint(&left_top);
    transform.TransformPoint(&right_bottom);
    transform.TransformPoint(&right_top);

    // Project point to Z near plane in view space.
    left_bottom.Scale(-z_near / left_bottom.z());
    left_top.Scale(-z_near / left_top.z());
    right_bottom.Scale(-z_near / right_bottom.z());
    right_top.Scale(-z_near / right_top.z());

    // Find bounding box on z near plane.
    float bounds_left = std::min(
        {left_bottom.x(), left_top.x(), right_bottom.x(), right_top.x()});
    float bounds_right = std::max(
        {left_bottom.x(), left_top.x(), right_bottom.x(), right_top.x()});
    float bounds_bottom = std::min(
        {left_bottom.y(), left_top.y(), right_bottom.y(), right_top.y()});
    float bounds_top = std::max(
        {left_bottom.y(), left_top.y(), right_bottom.y(), right_top.y()});

    // Ignore non visible elements.
    if (bounds_left >= z_near_right || bounds_right <= z_near_left ||
        bounds_bottom >= z_near_top || bounds_top <= z_near_bottom ||
        bounds_left == bounds_right || bounds_bottom == bounds_top) {
      continue;
    }

    // Clamp to Z near plane's boundary.
    bounds_left = base::ClampToRange(bounds_left, z_near_left, z_near_right);
    bounds_right = base::ClampToRange(bounds_right, z_near_left, z_near_right);
    bounds_bottom =
        base::ClampToRange(bounds_bottom, z_near_bottom, z_near_top);
    bounds_top = base::ClampToRange(bounds_top, z_near_bottom, z_near_top);

    left = std::min(bounds_left, left);
    right = std::max(bounds_right, right);
    bottom = std::min(bounds_bottom, bottom);
    top = std::max(bounds_top, top);
    has_visible_element = true;
  }

  if (!has_visible_element) {
    return FovRectangle{0.f, 0.f, 0.f, 0.f};
  }

  // Add a small margin to fix occasional border clipping due to precision.
  const float margin = std::tan(kMargin) * z_near;
  left = std::max(left - margin, z_near_left);
  right = std::min(right + margin, z_near_right);
  bottom = std::max(bottom - margin, z_near_bottom);
  top = std::min(top + margin, z_near_top);

  float left_degrees = std::atan(-left / z_near) * 180 / base::kPiFloat;
  float right_degrees = std::atan(right / z_near) * 180 / base::kPiFloat;
  float bottom_degrees = std::atan(-bottom / z_near) * 180 / base::kPiFloat;
  float top_degrees = std::atan(top / z_near) * 180 / base::kPiFloat;
  return FovRectangle{left_degrees, right_degrees, bottom_degrees, top_degrees};
}

#if defined(OS_ANDROID)
extern "C" {
// This symbol is retrieved from the VR feature module library via dlsym(),
// where it's bare address is type-cast to a CreateUiFunction pointer and
// executed. The forward declaration here ensures that the signatures match.
CreateUiFunction CreateUi;
__attribute__((visibility("default"))) UiInterface* CreateUi(
    UiBrowserInterface* browser,
    PlatformInputHandler* content_input_forwarder,
    std::unique_ptr<KeyboardDelegate> keyboard_delegate,
    std::unique_ptr<TextInputDelegate> text_input_delegate,
    std::unique_ptr<AudioDelegate> audio_delegate,
    const UiInitialState& ui_initial_state) {
  return new Ui(browser, content_input_forwarder, std::move(keyboard_delegate),
                std::move(text_input_delegate), std::move(audio_delegate),
                ui_initial_state);
}
}  // extern "C"
#endif  // defined(OS_ANDROID

}  // namespace vr
