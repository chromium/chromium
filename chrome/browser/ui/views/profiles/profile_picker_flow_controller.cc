// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_picker_flow_controller.h"

#include <string>
#include <variant>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/first_web_contents_profiler_base.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_metrics.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/signin_hats_util.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/profiles/profile_customization_util.h"
#include "chrome/browser/ui/signin/signin_view_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_flow_controller_impl.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_post_sign_in_adapter.h"
#include "chrome/browser/ui/views/profiles/profile_picker_reauth_provider.h"
#include "chrome/browser/ui/views/profiles/profile_picker_sign_in_provider.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"
#include "chrome/browser/ui/webui/signin/profile_picker_handler.h"
#include "chrome/browser/ui/webui/signin/signin_ui_error.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/public/base/signin_metrics.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/themes.mojom.h"

namespace {

const signin_metrics::AccessPoint kAccessPoint =
    signin_metrics::AccessPoint::kUserManager;

// Returns the URL to load as initial content for the profile picker. If an
// empty URL is returned, the profile picker should not be shown until
// another explicit call with a non-empty URL given to the view
// (see `ProfilePickerView::ShowScreen()` for example)
GURL GetInitialURL(ProfilePicker::EntryPoint entry_point) {
  GURL base_url = GURL(chrome::kChromeUIProfilePickerUrl);
  switch (entry_point) {
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile: {
      GURL::Replacements replacements;
      replacements.SetQueryStr(chrome::kChromeUIProfilePickerStartupQuery);
      return base_url.ReplaceComponents(replacements);
    }
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
      return base_url;
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail:
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
      return base_url.Resolve("new-profile");
    case ProfilePicker::EntryPoint::kFirstRun:
    case ProfilePicker::EntryPoint::kGlicManager:
      // Should not be used for this entry point.
      NOTREACHED();
  }
}

// Shows the customization bubble if possible. The bubble won't be shown if the
// color is enforced by policy or downloaded through Sync or the default theme
// should be used. An IPH is shown after the bubble, or right away if the bubble
// cannot be shown.
void ShowCustomizationBubble(std::optional<SkColor> new_profile_color,
                             Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view || !browser_view->toolbar_button_provider()) {
    return;
  }

  BrowserWindowFeatures& features = browser->GetFeatures();
  if (ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
          browser->profile())) {
    // For sync users, their profile color has not been applied yet. Call a
    // helper class that applies the color and shows the bubble only if there is
    // no conflict with a synced theme / color.
    features.profile_customization_bubble_sync_controller()
        ->ShowOnSyncFailedOrDefaultTheme(new_profile_color.value());
  } else {
    // For non syncing users, simply show the bubble.
    features.signin_view_controller()->ShowModalProfileCustomizationDialog();
  }
}

void MaybeShowProfileIPHs(Browser* browser) {
  DCHECK(browser);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }
  browser_view->MaybeShowSupervisedUserProfileSignInIPH();
  browser_view->MaybeShowProfileSwitchIPH();
}

// Class triggering the signed-in section of the profile management flow, most
// notably featuring the sync confirmation. In addition to what its base class
// `ProfilePickerPostSignInAdapter` is doing, this class:
// - shows in product help and customization bubble at the end of the flow
// - applies profile customizations (theme, profile name)
// - finalizes the profile (deleting it if the flow is aborted, marks it
//   non-ephemeral if the flow is completed)
// `step_completed_callback` is not called if the flow is canceled.
// Note that `account_id` has been added to the `IdentityManager` but may not
// be set as primary yet, because this operation is asynchronous.
class ProfileCreationPostSignInAdapter : public ProfilePickerPostSignInAdapter {
 public:
  ProfileCreationPostSignInAdapter(
      ProfilePickerWebContentsHost* host,
      Profile* profile,
      const CoreAccountInfo& account_info,
      std::unique_ptr<content::WebContents> contents,
      std::optional<SkColor> profile_color,
      base::OnceCallback<void(PostHostClearedCallback, bool)>
          step_completed_callback)
      : ProfilePickerPostSignInAdapter(host,
                                       profile,
                                       account_info,
                                       std::move(contents),
                                       kAccessPoint,
                                       profile_color),
        step_completed_callback_(std::move(step_completed_callback)) {}

  ProfileCreationPostSignInAdapter(const ProfileCreationPostSignInAdapter&) =
      delete;
  ProfileCreationPostSignInAdapter& operator=(
      const ProfileCreationPostSignInAdapter&) = delete;

  ~ProfileCreationPostSignInAdapter() override {
    // Record unfinished signed-in profile creation.
    if (!is_finishing_) {
      // TODO(crbug.com/40216113): Consider moving this recording into
      // ProfilePickerTurnSyncOnDelegate and unify this code with Cancel().
      ProfileMetrics::LogProfileAddSignInFlowOutcome(
          ProfileMetrics::ProfileSignedInFlowOutcome::kAbortedAfterSignIn);
    }
  }

  // ProfilePickerPostSignInAdapter:
  void Init(StepSwitchFinishedCallback step_switch_callback) override {
    // Listen for extended account info getting fetched.
    signin::IdentityManager* identity_manager =
        IdentityManagerFactory::GetForProfile(profile());
    profile_name_resolver_ =
        std::make_unique<ProfileNameResolver>(identity_manager, account_info());

    // Stop with the sign-in navigation and show a spinner instead. The spinner
    // will be shown until TurnSyncOnHelper figures out whether it's a
    // managed account and whether sync is disabled by policies (which in some
    // cases involves fetching policies and can take a couple of seconds).
    host()->ShowScreen(contents(), GetSyncConfirmationURL(/*loading=*/true),
                       base::OnceClosure());

    ProfilePickerPostSignInAdapter::Init(std::move(step_switch_callback));
  }

  void Cancel() override {
    if (is_finishing_) {
      return;
    }

    is_finishing_ = true;
  }

  void FinishAndOpenBrowserInternal(PostHostClearedCallback callback,
                                    bool is_continue_callback) override {
    // Do nothing if the sign-in flow is aborted or if this has already been
    // called. Note that this can get called first time from a special case
    // handling (such as the Settings link) and than second time when the
    // TurnSyncOnHelper finishes.
    if (is_finishing_) {
      return;
    }
    is_finishing_ = true;
    std::vector<PostHostClearedCallback> callbacks;
    callbacks.push_back(std::move(callback));
    callbacks.push_back(CreateFreshProfileExperienceCallback());
    callback = CombineCallbacks<PostHostClearedCallback, Browser*>(
        std::move(callbacks));

    profile_name_resolver_->RunWithProfileName(base::BindOnce(
        &ProfileCreationPostSignInAdapter::FinishFlow,
        // Unretained ok: `this` outlives `profile_name_resolver_`.
        base::Unretained(this), std::move(callback), is_continue_callback));
  }

 private:
  PostHostClearedCallback CreateFreshProfileExperienceCallback() {
    // If there's no color to apply to the profile, skip the customization
    // bubble and trigger an IPH, instead.
    if (ThemeServiceFactory::GetForProfile(profile())->UsingPolicyTheme() ||
        !GetProfileColor().has_value()) {
      return PostHostClearedCallback(base::BindOnce(&MaybeShowProfileIPHs));
    } else {
      // If sync cannot start, we apply `GetProfileColor()` right away before
      // opening a browser window to avoid flicker. Otherwise, it's applied
      // later by code triggered from ShowCustomizationBubble().
      if (!ProfileCustomizationBubbleSyncController::CanThemeSyncStart(
              profile())) {
        auto* theme_service = ThemeServiceFactory::GetForProfile(profile());
        theme_service->SetUserColorAndBrowserColorVariant(
            GetProfileColor().value(),
            ui::mojom::BrowserColorVariant::kTonalSpot);
      }
      return PostHostClearedCallback(
          base::BindOnce(&ShowCustomizationBubble, GetProfileColor()));
    }
  }

  void FinishFlow(PostHostClearedCallback post_host_cleared_callback,
                  bool is_continue_callback,
                  std::u16string name_for_signed_in_profile) {
    TRACE_EVENT1("browser", "ProfileCreationPostSignInAdapter::FinishFlow",
                 "profile_path", profile()->GetPath().AsUTF8Unsafe());
    CHECK(!name_for_signed_in_profile.empty());
    DCHECK(post_host_cleared_callback.value());
    DCHECK(step_completed_callback_);

    profile_name_resolver_.reset();

    FinalizeNewProfileSetup(profile(), name_for_signed_in_profile,
                            /*is_default_name=*/false);

    ProfileMetrics::LogProfileAddNewUser(
        ProfileMetrics::ADD_NEW_PROFILE_PICKER_SIGNED_IN);

    std::move(step_completed_callback_)
        .Run(std::move(post_host_cleared_callback), is_continue_callback);
  }

  // Controls whether the flow still needs to finalize (which includes showing
  // `profile` browser window at the end of the sign-in flow).
  bool is_finishing_ = false;

  std::unique_ptr<ProfileNameResolver> profile_name_resolver_;
  base::OnceCallback<void(PostHostClearedCallback, bool)>
      step_completed_callback_;
};

class ReauthFlowStepController : public ProfileManagementStepController {
 public:
  explicit ReauthFlowStepController(
      ProfilePickerWebContentsHost* host,
      std::unique_ptr<ProfilePickerReauthProvider> reauth_provider,
      Profile* profile)
      : ProfileManagementStepController(host),
        reauth_provider_(std::move(reauth_provider)) {}

  ~ReauthFlowStepController() override = default;

  void Show(StepSwitchFinishedCallback step_shown_callback,
            bool reset_state) override {
    reauth_provider_->SwitchToReauth(std::move(step_shown_callback));
  }

  void OnHidden() override { host()->SetNativeToolbarVisible(false); }

  void OnNavigateBackRequested() override {
    NavigateBackInternal(reauth_provider_->contents());
  }

 private:
  std::unique_ptr<ProfilePickerReauthProvider> reauth_provider_;
};

std::unique_ptr<ProfileManagementStepController> CreateReauthtep(
    ProfilePickerWebContentsHost* host,
    Profile* profile,
    base::OnceCallback<void(bool, const ForceSigninUIError&)>
        on_reauth_completed) {
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  return std::make_unique<ReauthFlowStepController>(
      host,
      std::make_unique<ProfilePickerReauthProvider>(
          host, profile, entry->GetGAIAId(),
          base::UTF16ToUTF8(entry->GetUserName()),
          std::move(on_reauth_completed)),
      profile);
}

void RecordProfilingFinishReason(
    metrics::StartupProfilingFinishReason finish_reason) {
  base::UmaHistogramEnumeration(
      "ProfilePicker.FirstProfileTime.FirstWebContentsFinishReason",
      finish_reason);
}

class FirstWebContentsProfilerForProfilePicker
    : public metrics::FirstWebContentsProfilerBase {
 public:
  explicit FirstWebContentsProfilerForProfilePicker(
      content::WebContents* web_contents,
      base::TimeTicks pick_time);

  FirstWebContentsProfilerForProfilePicker(
      const FirstWebContentsProfilerForProfilePicker&) = delete;
  FirstWebContentsProfilerForProfilePicker& operator=(
      const FirstWebContentsProfilerForProfilePicker&) = delete;

 protected:
  // FirstWebContentsProfilerBase:
  void RecordFinishReason(
      metrics::StartupProfilingFinishReason finish_reason) override;
  void RecordNavigationFinished(base::TimeTicks navigation_start) override;
  void RecordFirstNonEmptyPaint() override;
  bool WasStartupInterrupted() override;

 private:
  ~FirstWebContentsProfilerForProfilePicker() override;

  const base::TimeTicks pick_time_;
};

FirstWebContentsProfilerForProfilePicker::
    FirstWebContentsProfilerForProfilePicker(content::WebContents* web_contents,
                                             base::TimeTicks pick_time)
    : FirstWebContentsProfilerBase(web_contents), pick_time_(pick_time) {
  DCHECK(!pick_time_.is_null());
}

FirstWebContentsProfilerForProfilePicker::
    ~FirstWebContentsProfilerForProfilePicker() = default;

void FirstWebContentsProfilerForProfilePicker::RecordFinishReason(
    metrics::StartupProfilingFinishReason finish_reason) {
  RecordProfilingFinishReason(finish_reason);
}

void FirstWebContentsProfilerForProfilePicker::RecordNavigationFinished(
    base::TimeTicks navigation_start) {
  // Nothing to record here for Profile Picker startups.
}

void FirstWebContentsProfilerForProfilePicker::RecordFirstNonEmptyPaint() {
  const char histogram_name[] =
      "ProfilePicker.FirstProfileTime.FirstWebContentsNonEmptyPaint";
  base::TimeTicks paint_time = base::TimeTicks::Now();
  base::UmaHistogramLongTimes100(histogram_name, paint_time - pick_time_);
  TRACE_EVENT_BEGIN("startup", histogram_name,
                    perfetto::Track::FromPointer(this), pick_time_);
  TRACE_EVENT_END("startup", perfetto::Track::FromPointer(this), paint_time);
}

bool FirstWebContentsProfilerForProfilePicker::WasStartupInterrupted() {
  // We're assuming that no interruptions block opening an existing profile
  // from the profile picker. We would detect this by observing really high
  // latency on the tracked metric, and can start tracking interruptions if we
  // find that such cases occur.
  return false;
}

// Measures time to display the first web contents.
void BeginFirstWebContentsProfiling(Browser* browser,
                                    base::TimeTicks pick_time) {
  content::WebContents* visible_contents =
      metrics::FirstWebContentsProfilerBase::GetVisibleContents(browser);
  if (!visible_contents) {
    RecordProfilingFinishReason(metrics::StartupProfilingFinishReason::
                                    kAbandonNoInitiallyVisibleContent);
    return;
  }

  if (visible_contents->CompletedFirstVisuallyNonEmptyPaint()) {
    RecordProfilingFinishReason(
        metrics::StartupProfilingFinishReason::kAbandonAlreadyPaintedContent);
    return;
  }

  // FirstWebContentsProfilerForProfilePicker owns itself and is also bound to
  // |visible_contents|'s lifetime by observing WebContentsDestroyed().
  new FirstWebContentsProfilerForProfilePicker(visible_contents, pick_time);
}

void ShowLocalProfileCustomization(
    base::TimeTicks profile_picked_time_on_startup,
    Browser* browser) {
  if (!browser) {
    // TODO(crbug.com/40242414): Make sure we do something or log an error if
    // opening a browser window was not possible.
    return;
  }

  DCHECK(browser->window());
  Profile* profile = browser->profile();

  TRACE_EVENT1("browser", "ShowLocalProfileCustomization", "profile_path",
               profile->GetPath().AsUTF8Unsafe());

  if (!profile_picked_time_on_startup.is_null()) {
    BeginFirstWebContentsProfiling(browser, profile_picked_time_on_startup);
  }

  browser->GetFeatures()
      .signin_view_controller()
      ->ShowModalProfileCustomizationDialog(
          /*is_local_profile_creation=*/true);
}

void MaybeOpenPageInBrowser(Browser* browser,
                            const GURL& target_page_url,
                            bool open_settings) {
  // User clicked 'Edit' from the profile card menu.
  if (open_settings) {
    chrome::ShowSettingsSubPage(browser, chrome::kManageProfileSubPage);
    return;
  }

  // If no url is provided, proceed with the normal profile startup tabs
  // behaviour.
  if (target_page_url.is_empty()) {
    return;
  }

  // Opens the target url upon user selecting a pre-existing profile.
  if (target_page_url.spec() == chrome::kChromeUIHelpURL) {
    chrome::ShowAboutChrome(browser);
  } else if (target_page_url.spec() == chrome::kChromeUISettingsURL) {
    chrome::ShowSettings(browser);
  } else if (target_page_url.spec() == ProfilePicker::kTaskManagerUrl) {
    chrome::OpenTaskManager(browser);
  } else {
    ShowSingletonTabOverwritingNTP(browser, target_page_url);
  }
}

}  // namespace

ProfilePickerFlowController::ProfilePickerFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    ProfilePicker::EntryPoint entry_point,
    const GURL& selected_profile_target_url,
    const std::string& initial_email)
    : ProfileManagementFlowControllerImpl(
          host,
          std::move(clear_host_callback),
          /*flow_type_string=*/"ProfilePickerFlow"),
      entry_point_(entry_point),
      selected_profile_target_url_(selected_profile_target_url),
      initial_email_(initial_email) {}

ProfilePickerFlowController::~ProfilePickerFlowController() = default;

void ProfilePickerFlowController::Init() {
  RegisterStep(Step::kProfilePicker,
               ProfileManagementStepController::CreateForProfilePickerApp(
                   host(), GetInitialURL(entry_point_)));
  // If an initial email was provided, switch to the account selection step and
  // prefill the email field.
  if (!initial_email_.empty()) {
    SwitchToIdentityStepsFromAccountSelection(
        StepSwitchFinishedCallback(),
        signin_metrics::AccessPoint::kUserManagerWithPrefilledEmail,
        base::FilePath(), initial_email_);
    return;
  }
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
}

void ProfilePickerFlowController::SwitchToSignIn(
    ProfilePicker::ProfileInfo profile_info,
    StepSwitchFinishedCallback switch_finished_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  base::FilePath profile_path;
  // Split the variant information from `profile_info`.
  std::visit(absl::Overload{
                 [&suggested_profile_color =
                      suggested_profile_color_](std::optional<SkColor> color) {
                   suggested_profile_color = color;
                 },
                 [&profile_path](base::FilePath profile_path_info) {
                   profile_path = profile_path_info;
                 },
             },
             profile_info);

  SwitchToIdentityStepsFromAccountSelection(std::move(switch_finished_callback),
                                            kAccessPoint,
                                            std::move(profile_path));
}

void ProfilePickerFlowController::SwitchToReauth(
    Profile* profile,
    StepSwitchFinishedCallback switch_finished_callback,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback) {
  DCHECK_EQ(Step::kProfilePicker, current_step());

  // if the step was already initialized, unregister to make sure the new
  // reauth is properly initialised and the current reauth step is cleaned.
  //
  // TODO(crbug.com/40280498): Cleanup the unregistration of the step with a
  // proper resettable state within the `ProfilePickerReauthProvider`, and
  // using the `ProfileManagementFlowController::SwitchToStep()` `reset_state`
  // value to trigger the reset.
  if (IsStepInitialized(Step::kReauth)) {
    UnregisterStep(Step::kReauth);
  }

  RegisterStep(
      Step::kReauth,
      CreateReauthtep(
          host(), profile,
          base::BindOnce(&ProfilePickerFlowController::OnReauthCompleted,
                         base::Unretained(this), profile,
                         std::move(on_error_callback))));

  SwitchToStep(
      Step::kReauth, true, std::move(switch_finished_callback),
      /*pop_step_callback=*/CreateSwitchToStepPopCallback(current_step()));
}

void ProfilePickerFlowController::OnReauthCompleted(
    Profile* profile,
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
    bool success,
    const ForceSigninUIError& error) {
  if (!success) {
    CHECK_NE(error.type(), ForceSigninUIError::Type::kNone);

    SwitchToStep(
        Step::kProfilePicker, /*reset_state=*/true,
        StepSwitchFinishedCallback(base::BindOnce(
            &ProfilePickerFlowController::OnProfilePickerStepShownReauthError,
            base::Unretained(this), std::move(on_error_callback), error)));
    return;
  }

  g_browser_process->profile_manager()
      ->GetProfileAttributesStorage()
      .GetProfileAttributesWithPath(profile->GetPath())
      ->LockForceSigninProfile(false);

  FinishFlowAndRunInBrowser(profile, PostHostClearedCallback());
}

void ProfilePickerFlowController::OnProfilePickerStepShownReauthError(
    base::OnceCallback<void(const ForceSigninUIError&)> on_error_callback,
    const ForceSigninUIError& error,
    bool switch_step_success) {
  // If the step switch to the profile picker was not successful, do not proceed
  // with displaying the error dialog.
  if (!switch_step_success) {
    return;
  }

  std::move(on_error_callback).Run(error);
}

void ProfilePickerFlowController::CancelSigninFlow() {
  // Triggered from either entreprise welcome or profile switch screens.
  DCHECK(current_step() == Step::kPostSignInFlow ||
         current_step() == Step::kAccountSelection);

  switch (entry_point_) {
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOnStartup:
    case ProfilePicker::EntryPoint::kOnStartupNoProfile:
    case ProfilePicker::EntryPoint::kProfileMenuManageProfiles:
    case ProfilePicker::EntryPoint::kOpenNewWindowAfterProfileDeletion:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcess:
    case ProfilePicker::EntryPoint::kNewSessionOnExistingProcessNoProfile:
    case ProfilePicker::EntryPoint::kProfileLocked:
    case ProfilePicker::EntryPoint::kUnableToCreateBrowser:
    case ProfilePicker::EntryPoint::kBackgroundModeManager:
    case ProfilePicker::EntryPoint::kProfileIdle:
    case ProfilePicker::EntryPoint::kOnStartupCreateProfileWithEmail: {
      SwitchToStep(Step::kProfilePicker, /*reset_state=*/true);
      UnregisterStep(Step::kPostSignInFlow);
      UnregisterStep(Step::kAccountSelection);
      return;
    }
    case ProfilePicker::EntryPoint::kAppMenuProfileSubMenuAddNewProfile:
    case ProfilePicker::EntryPoint::kProfileMenuAddNewProfile: {
      // This results in destroying `this`.
      ExitFlow();
      return;
    }
    case ProfilePicker::EntryPoint::kFirstRun:
    case ProfilePicker::EntryPoint::kGlicManager:
      NOTREACHED() << "CancelSigninFlow() is not reachable from "
                      "this entry point";
  }
}

std::u16string ProfilePickerFlowController::GetFallbackAccessibleWindowTitle()
    const {
  return l10n_util::GetStringUTF16(IDS_PROFILE_PICKER_MAIN_VIEW_TITLE);
}

std::unique_ptr<ProfilePickerPostSignInAdapter>
ProfilePickerFlowController::CreatePostSignInAdapter(
    Profile* signed_in_profile,
    const CoreAccountInfo& account_info,
    std::unique_ptr<content::WebContents> contents) {
  DCHECK(!weak_post_sign_in_adapter_);

  created_profile_ = signed_in_profile->GetWeakPtr();
  auto step_completed_callback =
      base::BindOnce(&ProfilePickerFlowController::HandleIdentityStepsCompleted,
                     // Unretained ok: the callback is passed to a step that
                     // the `this` will own and outlive.
                     base::Unretained(this),
                     // Unretained ok: the steps register a profile keep-alive
                     // and will be alive until this callback runs.
                     base::Unretained(created_profile_.get()));

  auto signed_in_flow = std::make_unique<ProfileCreationPostSignInAdapter>(
      host(), signed_in_profile, account_info, std::move(contents),
      suggested_profile_color_, std::move(step_completed_callback));
  weak_post_sign_in_adapter_ = signed_in_flow->GetWeakPtr();
  return signed_in_flow;
}

void ProfilePickerFlowController::SwitchToSignedOutPostIdentityFlow(
    Profile* profile) {
  CHECK(profile);
  created_profile_ = profile->GetWeakPtr();
  CreateSignedOutFlowWebContents(created_profile_.get());

  HandleIdentityStepsCompleted(
      created_profile_.get(),
      PostHostClearedCallback(base::BindOnce(&ShowLocalProfileCustomization,
                                             profile_picked_time_on_startup_)),
      /*is_continue_callback=*/false);
}

void ProfilePickerFlowController::PickProfile(
    const base::FilePath& profile_path,
    ProfilePicker::ProfilePickingArgs args,
    base::OnceCallback<void(bool)> pick_profile_complete_callback) {
  if (args.should_record_startup_metrics &&
      // Avoid overriding the picked time if already recorded. This can happen
      // for example if multiple profiles are picked: https://crbug.com/1277466.
      profile_picked_time_on_startup_.is_null()) {
    profile_picked_time_on_startup_ = base::TimeTicks::Now();
  }

  bool open_command_line_urls = ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened();
  ProfilePicker::SetOpenCommandLineUrlsInNextProfileOpened(false);

  profiles::SwitchToProfile(
      profile_path, /*always_create=*/false,
      base::BindOnce(&ProfilePickerFlowController::OnSwitchToProfileComplete,
                     weak_ptr_factory_.GetWeakPtr(), args.open_settings,
                     args.exit_flow_after_profile_picked,
                     std::move(pick_profile_complete_callback)),
      open_command_line_urls);
}

void ProfilePickerFlowController::OnSwitchToProfileComplete(
    bool open_settings,
    bool exit_flow_after_profile_picked,
    base::OnceCallback<void(bool)> pick_profile_complete_callback,
    Browser* browser) {
  if (!browser || browser->is_delete_scheduled()) {
    // The browser is destroyed or about to be destroyed.
    if (pick_profile_complete_callback) {
      std::move(pick_profile_complete_callback).Run(false);
    }
    return;
  }

  DCHECK(browser->window());
  if (pick_profile_complete_callback) {
    std::move(pick_profile_complete_callback).Run(true);
  }
  Profile* profile = browser->profile();
  TRACE_EVENT1("browser",
               "ProfilePickerFlowController::OnSwitchToProfileComplete",
               "profile_path", profile->GetPath().AsUTF8Unsafe());

  // Measure startup time to display first web contents if the profile picker
  // was displayed on startup and if the initiating action is instrumented. For
  // example we don't record pick time for profile creations.
  if (!profile_picked_time_on_startup_.is_null()) {
    BeginFirstWebContentsProfiling(browser, profile_picked_time_on_startup_);
  }

  // Only show the profile switch IPH when the user clicked the card, and there
  // are multiple profiles.
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributes();
  int profile_count =
      std::ranges::count(entries, false, &ProfileAttributesEntry::IsOmitted);
  if (profile_count > 1 && !open_settings &&
      selected_profile_target_url_.is_empty()) {
    browser->window()->MaybeShowProfileSwitchIPH();
  }

  if (profile->IsGuestSession()) {
    RecordProfilePickerAction(ProfilePickerAction::kLaunchGuestProfile);
  } else {
    // Launch a HaTS survey if the user intentionally switched profiles via the
    // profile picker accessed from the profile menu. This excludes first-run or
    // startup scenarios.
    if (entry_point_ == ProfilePicker::EntryPoint::kProfileMenuManageProfiles) {
      signin::LaunchSigninHatsSurveyForProfile(
          kHatsSurveyTriggerIdentitySwitchProfileFromProfilePicker, profile);
    }
    RecordProfilePickerAction(
        open_settings
            ? ProfilePickerAction::kLaunchExistingProfileCustomizeSettings
            : ProfilePickerAction::kLaunchExistingProfile);
  }

  MaybeOpenPageInBrowser(browser, selected_profile_target_url_, open_settings);
  // Closes the Profile Picker.
  //
  // Making sure the flow has not already exited here is needed, because
  // potentially this specific flow can be run twice by the time the host was
  // cleared; e.g. by clicking quickly multiple times when picking a Profile
  // from the Profile Picker view.
  // Depending on the state of the first call, subsequent calls may result in
  // `BeginFirstWebContentsProfiling()` recording multiple metrics.
  // TODO(crbug.com/389887233): Investigate further how often this happens to
  // consider having a better architecture to avoid those issues with multiple
  // flow-exiting calls being executed at the same time.
  if (!HasFlowExited() && exit_flow_after_profile_picked) {
    ExitFlow();
  }
}

base::queue<ProfileManagementFlowController::Step>
ProfilePickerFlowController::RegisterPostIdentitySteps(
    PostHostClearedCallback post_host_cleared_callback) {
  CHECK(created_profile_);
  base::queue<ProfileManagementFlowController::Step> post_identity_steps;

  content::WebContents* web_contents = nullptr;
  if (weak_post_sign_in_adapter_) {
    // TODO(crbug.com/40942098): Find a way to get the web contents without
    // relying on the weak ptr.
    web_contents = weak_post_sign_in_adapter_->contents();
    CHECK(web_contents);
  } else {
    // TODO(crbug.com/40942098): Find another way to fetch the web contents.
    web_contents = GetSignedOutFlowWebContents();
    CHECK(web_contents);
  }

  auto search_engine_choice_step_completed = base::BindOnce(
      &ProfilePickerFlowController::AdvanceToNextPostIdentityStep,
      base::Unretained(this));
  SearchEngineChoiceDialogService* search_engine_choice_dialog_service =
      SearchEngineChoiceDialogServiceFactory::GetForProfile(
          created_profile_.get());
  RegisterStep(
      Step::kSearchEngineChoice,
      ProfileManagementStepController::CreateForSearchEngineChoice(
          host(), search_engine_choice_dialog_service, web_contents,
          SearchEngineChoiceDialogService::EntryPoint::kProfileCreation,
          std::move(search_engine_choice_step_completed)));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kSearchEngineChoice);

  RegisterStep(
      Step::kFinishFlow,
      ProfileManagementStepController::CreateForFinishFlowAndRunInBrowser(
          host(),
          base::BindOnce(
              &ProfilePickerFlowController::FinishFlowAndRunInBrowser,
              base::Unretained(this), base::Unretained(created_profile_.get()),
              std::move(post_host_cleared_callback))));
  post_identity_steps.emplace(
      ProfileManagementFlowController::Step::kFinishFlow);

  return post_identity_steps;
}
