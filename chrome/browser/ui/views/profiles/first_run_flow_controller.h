// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/profiles/profile_picker.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_toolbar.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/intro/finish_or_continue_handler.h"
#include "services/audio/public/cpp/sounds/sounds_manager.h"

struct CoreAccountInfo;
enum class IntroChoice;
class FeatureShowcaseStepController;
class Profile;

// Exposed for testing purposes only.
// These values are persisted to UMA logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(FeatureShowcaseStep)
enum class FeatureShowcaseStep {
  kDefaultBrowser = 0,
  kGoogleLens = 1,
  kPasswordManager = 2,
  kThemesAndCustomization = 3,
  kMaxValue = kThemesAndCustomization,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/enums.xml:FeatureShowcaseStep)

// Creates a step to represent the intro. Exposed for testing.
std::unique_ptr<ProfileManagementStepController> CreateIntroStep(
    ProfilePickerWebContentsHost* host,
    base::RepeatingCallback<void(IntroChoice)> choice_callback,
    bool enable_animations,
    base::RepeatingCallback<bool()> query_effects_callback);

std::unique_ptr<ProfileManagementStepController> CreateDefaultBrowserStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback);

std::unique_ptr<ProfileManagementStepController> CreateFeatureShowcaseStep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceClosure step_completed_callback = base::DoNothing(),
    base::RepeatingClosure play_progress_sound_callback = base::DoNothing(),
    base::RepeatingCallback<void(bool)> toggle_ambient_sound_callback =
        base::DoNothing());

std::unique_ptr<ProfileManagementStepController> CreateFinishOrContinueStep(
    ProfilePickerWebContentsHost* host,
    base::OnceCallback<bool()> eligibility_callback,
    base::RepeatingCallback<bool()> query_effects_callback,
    base::OnceCallback<void(FinishOrContinueChoice)> step_completed_callback,
    base::OnceClosure play_all_set_sound_callback);

class FirstRunFlowController : public ProfileManagementFlowControllerImpl {
 public:
  static constexpr audio::SoundsManager::SoundKey kAmbientSoundKey = 0;
  static constexpr audio::SoundsManager::SoundKey kLogoSoundKey = 1;
  static constexpr audio::SoundsManager::SoundKey kWelcomeBackSoundKey = 2;
  static constexpr audio::SoundsManager::SoundKey
      kFeatureShowcaseAmbientSoundKey = 3;
  static constexpr audio::SoundsManager::SoundKey
      kFeatureShowcaseProgressSoundKey = 4;
  static constexpr audio::SoundsManager::SoundKey kAllSetSoundKey = 5;

  // Profile management flow controller that will run the FRE for `profile` in
  // `host`.
  FirstRunFlowController(
      ProfilePickerWebContentsHost* host,
      ClearHostClosure clear_host_callback,
      Profile* profile,
      ProfilePicker::FirstRunExitedCallback first_run_exited_callback);
  ~FirstRunFlowController() override;

  // ProfileManagementFlowControllerImpl:
  void Init() override;
  void CancelSigninFlow() override;
  void PickProfile(
      const base::FilePath& profile_path,
      ProfilePicker::ProfilePickingArgs args,
      base::OnceCallback<void(bool)> pick_profile_complete_callback) override;
  void ShowSigninError(Profile* profile, const SigninUIError& error) override;
  ProfilePickerToolbar::Builder CreateToolbarBuilder() override;

  using SoundsManagerFactory =
      base::RepeatingCallback<std::unique_ptr<audio::SoundsManager>(
          audio::SoundsManager::StreamFactoryBinder)>;

  // Sets the factory used to create the `SoundsManager` in tests.
  // The returned `base::AutoReset` automatically restores the previous factory
  // when it goes out of scope.
  [[nodiscard]] static base::AutoReset<SoundsManagerFactory>
  SetSoundsManagerFactoryForTesting(SoundsManagerFactory factory);

 protected:
  // ProfileManagementFlowControllerImpl
  bool PreFinishWithBrowser() override;
  // `account_info` may not be set as the primary account yet.
  std::unique_ptr<ProfilePickerPostSignInAdapter> CreatePostSignInAdapter(
      Profile* signed_in_profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents) override;
  base::queue<ProfileManagementFlowController::Step> RegisterPostIdentitySteps(
      PostHostClearedCallback post_host_cleared_callback) override;

 private:
  bool is_feature_showcase_eligible() const;

  void HandleIntroSigninChoice(IntroChoice choice);

  void PlaySignInCelebrationSound();

  void StartBrowsing();

  // Run the `finish_flow_callback_` if it's not empty.
  void RunFinishFlowCallback();

  std::string GetHatsSurveyTrigger() const;

  void ToggleMediaEffects(bool active);

  void UpdateAmbientSound(audio::SoundsManager::SoundKey ambient_sound_key);

  void ToggleFeatureShowcaseAmbientSound(bool active);

  void PlayFeatureShowcaseProgressSound();

  void PlayAllSetSound();

  bool AreEffectsEnabled() const;

  void MaybeTriggerHatsSurvey();

  void OnFlowFinished(PostHostClearedCallback post_host_cleared_callback);
  void OnFinishOrContinueChoice(FinishOrContinueChoice choice);

  const raw_ptr<Profile> profile_;
  ProfilePicker::FirstRunExitedCallback first_run_exited_callback_;

  FinishOrContinueChoice finish_or_continue_choice_ =
      FinishOrContinueChoice::kStartBrowsing;

  // The callback that will finish the flow and open the browser.
  base::OnceClosure finish_flow_callback_;

  std::unique_ptr<audio::SoundsManager> sounds_manager_;
  audio::SoundsManager::SoundKey ambient_sound_key_ = kAmbientSoundKey;

  base::WeakPtr<FeatureShowcaseStepController>
      feature_showcase_step_controller_;

  base::WeakPtrFactory<FirstRunFlowController> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PROFILES_FIRST_RUN_FLOW_CONTROLLER_H_
