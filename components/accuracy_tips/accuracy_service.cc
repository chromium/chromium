// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accuracy_tips/accuracy_service.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/containers/contains.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/accuracy_tips/accuracy_tip_interaction.h"
#include "components/accuracy_tips/accuracy_tip_safe_browsing_client.h"
#include "components/accuracy_tips/accuracy_tip_status.h"
#include "components/accuracy_tips/features.h"
#include "components/accuracy_tips/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace accuracy_tips {

// Returns a suffix for accuracy tips histograms.
// Needs to match AccuracyTipInteractions from histogram_suffixes_list.xml.
const std::string GetHistogramSuffix(AccuracyTipInteraction interaction) {
  switch (interaction) {
    case AccuracyTipInteraction::kNoAction:
      return "NoAction";
    case AccuracyTipInteraction::kLearnMore:
      return "LearnMore";
    case AccuracyTipInteraction::kOptOut:
      return "OptOut";
    case AccuracyTipInteraction::kClosed:
      return "Closed";
    case AccuracyTipInteraction::kDisabledByExperiment:
      NOTREACHED();  // We don't need specific histograms for this.
      return "";
  }
}

// Returns the preference to store feature state in. Uses a different pref if
// the UI is disabled to avoid dark-launch experiments affecting real usage.
const char* GetLastShownPrefName(bool disable_ui) {
  return disable_ui ? prefs::kLastAccuracyTipShownDisabledUi
                    : prefs::kLastAccuracyTipShown;
}
const char* GetPreviousInteractionsPrefName(bool disable_ui) {
  return disable_ui ? prefs::kPreviousInteractionsDisabledUi
                    : prefs::kPreviousInteractions;
}

using AccuracyCheckCallback = AccuracyService::AccuracyCheckCallback;

// static
void AccuracyService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  const bool disable_ui = features::kDisableUi.Get();
  registry->RegisterTimePref(GetLastShownPrefName(disable_ui), base::Time());
  registry->RegisterListPref(GetPreviousInteractionsPrefName(disable_ui));
}

AccuracyService::AccuracyService(
    std::unique_ptr<Delegate> delegate,
    PrefService* pref_service,
    scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager> sb_database,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : delegate_(std::move(delegate)),
      pref_service_(pref_service),
      ui_task_runner_(ui_task_runner),
      sample_url_(GURL(features::kSampleUrl.Get())),
      time_between_prompts_(features::kTimeBetweenPrompts.Get()),
      disable_ui_(features::kDisableUi.Get()) {
  if (sb_database) {
    sb_client_ = base::MakeRefCounted<AccuracyTipSafeBrowsingClient>(
        std::move(sb_database), std::move(ui_task_runner),
        std::move(io_task_runner));
  }
}

AccuracyService::~AccuracyService() = default;

void AccuracyService::Shutdown() {
  if (sb_client_) {
    sb_client_->Shutdown();
    sb_client_ = nullptr;
  }
}

void AccuracyService::CheckAccuracyStatus(const GURL& url,
                                          AccuracyCheckCallback callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  const base::Value* last_interactions =
      pref_service_->Get(GetPreviousInteractionsPrefName(disable_ui_));
  const base::Value opt_out_value(
      static_cast<int>(AccuracyTipInteraction::kOptOut));
  if (base::Contains(last_interactions->GetList(), opt_out_value)) {
    return std::move(callback).Run(AccuracyTipStatus::kOptOut);
  }

  base::Time last_shown =
      pref_service_->GetTime(GetLastShownPrefName(disable_ui_));
  if (clock_->Now() - last_shown < time_between_prompts_) {
    return std::move(callback).Run(AccuracyTipStatus::kRateLimited);
  }

  if (sample_url_.is_valid() && url == sample_url_) {
    return OnAccuracyStatusReceived(url, std::move(callback),
                                    AccuracyTipStatus::kShowAccuracyTip);
  }

  if (!sb_client_) {
    return std::move(callback).Run(AccuracyTipStatus::kNone);
  }

  sb_client_->CheckAccuracyStatus(
      url,
      base::BindOnce(&AccuracyService::OnAccuracyStatusReceived,
                     weak_factory_.GetWeakPtr(), url, std::move(callback)));
}

void AccuracyService::OnAccuracyStatusReceived(const GURL& url,
                                               AccuracyCheckCallback callback,
                                               AccuracyTipStatus status) {
  if (status == AccuracyTipStatus::kShowAccuracyTip &&
      delegate_->IsEngagementHigh(url)) {
    return std::move(callback).Run(AccuracyTipStatus::kHighEnagagement);
  }
  return std::move(callback).Run(status);
}

void AccuracyService::MaybeShowAccuracyTip(content::WebContents* web_contents) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  pref_service_->SetTime(GetLastShownPrefName(disable_ui_), clock_->Now());

  if (disable_ui_) {
    return OnAccuracyTipClosed(base::TimeTicks(),
                               AccuracyTipInteraction::kDisabledByExperiment);
  }

  delegate_->ShowAccuracyTip(
      web_contents, AccuracyTipStatus::kShowAccuracyTip,
      base::BindOnce(&AccuracyService::OnAccuracyTipClosed,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void AccuracyService::OnAccuracyTipClosed(base::TimeTicks time_opened,
                                          AccuracyTipInteraction interaction) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  ListPrefUpdate update(pref_service_,
                        GetPreviousInteractionsPrefName(disable_ui_));
  base::Value* interaction_list = update.Get();
  interaction_list->Append(static_cast<int>(interaction));

  // Record metrics.
  base::UmaHistogramEnumeration("Privacy.AccuracyTip.AccuracyTipInteraction",
                                interaction);
  base::UmaHistogramCounts100("Privacy.AccuracyTip.NumDialogsShown",
                              interaction_list->GetList().size());

  if (interaction != AccuracyTipInteraction::kDisabledByExperiment) {
    const base::TimeDelta time_open = base::TimeTicks::Now() - time_opened;
    base::UmaHistogramMediumTimes("Privacy.AccuracyTip.AccuracyTipTimeOpen",
                                  time_open);

    const std::string suffix = GetHistogramSuffix(interaction);
    base::UmaHistogramCounts100("Privacy.AccuracyTip.NumDialogsShown." + suffix,
                                interaction_list->GetList().size());
    base::UmaHistogramMediumTimes(
        "Privacy.AccuracyTip.AccuracyTipTimeOpen." + suffix, time_open);
  }
}

}  // namespace accuracy_tips
