// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/starter_platform_delegate.h"
#include "components/autofill_assistant/browser/startup_util.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

namespace autofill_assistant {

// Starts autofill-assistant flows. Uses a platform delegate to show UI and
// access platform-dependent features.
class Starter : public content::WebContentsObserver {
 public:
  // Note: |url| and |trigger_context| are only valid and not null if
  // |start_regular_script| is true.
  using StarterResultCallback =
      base::OnceCallback<void(bool start_regular_script,
                              GURL url,
                              std::unique_ptr<TriggerContext> trigger_context)>;

  explicit Starter(content::WebContents* web_contents,
                   StarterPlatformDelegate* platform_delegate,
                   ukm::UkmRecorder* ukm_recorder);
  ~Starter() override;
  Starter(const Starter&) = delete;
  Starter& operator=(const Starter&) = delete;

  // Entry-point for all non-direct-action flows. This method will perform the
  // overall startup procedure, which is roughly as follows:
  //  - Check parameters/features/settings and determine startup mode
  //  - Install feature module if necessary
  //  - Run and wait for trigger script to finish if necessary
  //  - Show onboarding if necessary
  //  - Invoke |callback| with the result. On success, the caller should start
  // the regular script. TODO(mcarlen): client startup should also be in
  // handled here, rather than in the caller.
  //
  // Only one call to |Start| can be processed at any time. If this method is
  // called before the previous call has finished, the previous call is
  // cancelled.
  void Start(std::unique_ptr<TriggerContext> trigger_context,
             StarterResultCallback callback);

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Re-check settings. This may cancel ongoing startup requests if the required
  // settings are no longer enabled.
  void CheckSettings();

 private:
  // Cancels the currently pending startup request, if any.
  void CancelPendingStartup();

  // Installs the feature module if necessary, otherwise directly invokes
  // |OnFeatureModuleInstalled|.
  void MaybeInstallFeatureModule(StartupUtil::StartupMode startup_mode);

  // Stops the startup if the installation failed. Otherwise, proceeds with the
  // next step of the startup process.
  void OnFeatureModuleInstalled(StartupUtil::StartupMode startup_mode,
                                Metrics::FeatureModuleInstallation result);

  // Starts a trigger script and waits for it to finish in
  // |OnTriggerScriptFinished|.
  void StartTriggerScript();

  // Stops the startup if the trigger script failed or was user-cancelled.
  // Otherwise, proceeds with the start of the regular script.
  void OnTriggerScriptFinished(
      Metrics::LiteScriptFinishedState state,
      base::Optional<TriggerScriptProto> trigger_script);

  // Shows the onboarding if necessary, otherwise directly invokes
  // |OnOnboardingFinished|.
  void MaybeShowOnboarding();

  // Starts the regular script if onboarding was accepted. Stops the startup
  // process if onboarding was rejected.
  void OnOnboardingFinished(bool shown, OnboardingResult result);

  // Internal helper to invoke the pending callback.
  void RunCallback(bool start_regular_script);

  StarterPlatformDelegate* platform_delegate_;
  ukm::UkmRecorder* ukm_recorder_ = nullptr;
  bool fetch_trigger_scripts_on_navigation_ = false;
  std::unique_ptr<TriggerContext> pending_trigger_context_;
  StarterResultCallback pending_callback_;
  base::WeakPtrFactory<Starter> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STARTER_H_
