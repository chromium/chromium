// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/default_browser_handler.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "chrome/browser/ui/webui/intro/intro_ui.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/taskbar_manager.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/shell_util.h"
#endif

namespace {

bool IsDefaultBrowserDisabledByPolicy() {
  const PrefService::Preference* pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kDefaultBrowserSettingEnabled);
  CHECK(pref);
  CHECK(pref->GetValue()->is_bool());
  return pref->IsManaged() && !pref->GetValue()->GetBool();
}

// TODO(crbug.com/505629973): Check how to record other metrics as 'Quit'.
void MaybeLogSetAsDefaultSuccess(
    shell_integration::DefaultWebClientState state) {
  if (state == shell_integration::IS_DEFAULT) {
    base::UmaHistogramEnumeration(
        "ProfilePicker.FirstRun.DefaultBrowser",
        DefaultBrowserChoice::kSuccessfullySetAsDefault);
  }
}

}  // namespace

DefaultBrowserHandler::DefaultBrowserHandler(
    mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
        receiver)
    : receiver_(this, std::move(receiver)) {}

DefaultBrowserHandler::DefaultBrowserHandler(
    mojo::PendingReceiver<feature_showcase::mojom::DefaultBrowserPageHandler>
        receiver,
    base::OnceClosure on_set_as_default_completed_callback
#if BUILDFLAG(IS_WIN)
    ,
    PinToTaskbarCallbackForTesting on_pin_to_taskbar_callback
#endif
    )
    : receiver_(this, std::move(receiver)),
      on_set_as_default_completed_callback_for_testing_(
          std::move(on_set_as_default_completed_callback))
#if BUILDFLAG(IS_WIN)
      ,
      on_pin_to_taskbar_callback_for_testing_(
          std::move(on_pin_to_taskbar_callback))
#endif
{
}

DefaultBrowserHandler::~DefaultBrowserHandler() = default;

void DefaultBrowserHandler::SetAsDefaultBrowser() {
  RecordStepUserAction(FeatureShowcaseStep::kDefaultBrowser,
                       FeatureShowcaseStepUserAction::kAccepted);
  // TODO(crbug.com/486819807): Remove old metrics once we completely switch to
  // the new flow.
  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                DefaultBrowserChoice::kClickSetAsDefault);

  CHECK(!IsDefaultBrowserDisabledByPolicy());
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartSetAsDefault(base::BindOnce(
          [](base::OnceClosure on_set_as_default_completed_callback_for_testing,
             shell_integration::DefaultWebClientState state) {
            MaybeLogSetAsDefaultSuccess(state);
            if (on_set_as_default_completed_callback_for_testing) {
              std::move(on_set_as_default_completed_callback_for_testing).Run();
            }
          },
          std::move(on_set_as_default_completed_callback_for_testing_)));

#if BUILDFLAG(IS_WIN)
  if (can_pin_) {
    browser_util::PinResultCallback pin_callback = base::DoNothing();
    if (on_pin_to_taskbar_callback_for_testing_) {
      pin_callback = std::move(on_pin_to_taskbar_callback_for_testing_);
    }
    browser_util::PinAppToTaskbar(
        ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall()),
        browser_util::PinAppToTaskbarChannel::kFirstRunExperience,
        std::move(pin_callback));
  }
#endif
}

void DefaultBrowserHandler::SkipSetAsDefaultBrowser() {
  RecordStepUserAction(FeatureShowcaseStep::kDefaultBrowser,
                       FeatureShowcaseStepUserAction::kDeclined);
  // TODO(crbug.com/486819807): Remove old metrics once we completely switch to
  // the new flow.
  base::UmaHistogramEnumeration("ProfilePicker.FirstRun.DefaultBrowser",
                                DefaultBrowserChoice::kSkip);
}

void DefaultBrowserHandler::SetCanPin(bool can_pin) {
  can_pin_ = can_pin;
}
