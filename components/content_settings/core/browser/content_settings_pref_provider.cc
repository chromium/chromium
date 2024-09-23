// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_pref.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_partition_key.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "services/preferences/public/cpp/dictionary_value_update.h"
#include "services/preferences/public/cpp/scoped_pref_update.h"
#include "services/tracing/public/cpp/perfetto/macros.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_content_settings_event_info.pbzero.h"

namespace content_settings {

namespace {

// These settings are no longer used, and should be deleted on profile startup.

#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
const char kObsoleteInstalledWebAppMetadataExceptionsPref[] =
    "profile.content_settings.exceptions.installed_web_app_metadata";
const char kObsoletePpapiBrokerExceptionsPref[] =
    "profile.content_settings.exceptions.ppapi_broker";
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
const char
    kObsoleteGetDisplayMediaSetAutoSelectAllScreensAllowedForUrlsExceptionsPref
        [] = "profile.content_settings.exceptions.get_display_media_set_select_"
             "all_screens";
constexpr char kObsoleteFederatedIdentityActiveSesssionExceptionsPref[] =
    "profile.content_settings.exceptions.fedcm_active_session";

#if !BUILDFLAG(IS_IOS)
// This setting was accidentally bound to a UI surface intended for a different
// setting (https://crbug.com/364820109). It should not have been settable
// except via enterprise policy, so it is temporarily cleaned up here to revert
// it to its default value.
// TODO(https://crbug.com/367181093): clean this up.
constexpr char kBug364820109ExceptionSettingToClear[] =
    "profile.content_settings.exceptions.javascript_jit";
constexpr char kBug364820109AlreadyWorkedAroundPref[] =
    "profile.did_work_around_bug_364820109_exceptions";
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

// ////////////////////////////////////////////////////////////////////////////
// PrefProvider:
//

// static
void PrefProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kContentSettingsVersion,
      ContentSettingsPattern::kContentSettingsPatternVersion);
  registry->RegisterBooleanPref(prefs::kInContextCookieControlsOpened, false);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    registry->RegisterDictionaryPref(info->pref_name(),
                                     info->GetPrefRegistrationFlags());
    registry->RegisterDictionaryPref(info->partitioned_pref_name(),
                                     info->GetPrefRegistrationFlags());
  }

  // Obsolete prefs ----------------------------------------------------------

  // These prefs have been removed, but need to be registered so they can
  // be deleted on startup.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterDictionaryPref(
      kObsoleteInstalledWebAppMetadataExceptionsPref);
  registry->RegisterDictionaryPref(kObsoletePpapiBrokerExceptionsPref);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  registry->RegisterListPref(
      kObsoleteGetDisplayMediaSetAutoSelectAllScreensAllowedForUrlsExceptionsPref);
  registry->RegisterListPref(
      kObsoleteFederatedIdentityActiveSesssionExceptionsPref);
#if !BUILDFLAG(IS_IOS)
  // TODO(https://crbug.com/367181093): clean this up.
  registry->RegisterBooleanPref(kBug364820109AlreadyWorkedAroundPref, false);
#endif  // !BUILDFLAG(IS_IOS)
}

PrefProvider::PrefProvider(PrefService* prefs,
                           bool off_the_record,
                           bool store_last_modified,
                           bool restore_session)
    : prefs_(prefs),
      off_the_record_(off_the_record),
      store_last_modified_(store_last_modified),
      clock_(base::DefaultClock::GetInstance()) {
  TRACE_EVENT_BEGIN("startup", "PrefProvider::PrefProvider");
  DCHECK(prefs_);
  // Verify preferences version.
  if (!prefs_->HasPrefPath(prefs::kContentSettingsVersion)) {
    prefs_->SetInteger(prefs::kContentSettingsVersion,
                       ContentSettingsPattern::kContentSettingsPatternVersion);
  }
  if (prefs_->GetInteger(prefs::kContentSettingsVersion) >
      ContentSettingsPattern::kContentSettingsPatternVersion) {
    TRACE_EVENT_END("startup");  // PrefProvider::PrefProvider.
    return;
  }

  DiscardOrMigrateObsoletePreferences();

  pref_change_registrar_.Init(prefs_);

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    content_settings_prefs_.insert(std::make_pair(
        info->type(),
        std::make_unique<ContentSettingsPref>(
            info->type(), prefs_, &pref_change_registrar_, info->pref_name(),
            info->partitioned_pref_name(), off_the_record_, restore_session,
            base::BindRepeating(&PrefProvider::Notify,
                                base::Unretained(this)))));
  }

  size_t num_exceptions = 0;
  if (!off_the_record_) {
    for (const auto& pref : content_settings_prefs_) {
      num_exceptions += pref.second->GetNumExceptions();
    }

    UMA_HISTOGRAM_COUNTS_1M("ContentSettings.NumberOfExceptions",
                            num_exceptions);
  }

  TRACE_EVENT_END("startup", [num_exceptions](perfetto::EventContext ctx) {
    perfetto::protos::pbzero::ChromeContentSettingsEventInfo* event_args =
        ctx.event()->set_chrome_content_settings_event_info();
    event_args->set_number_of_exceptions(
        num_exceptions);  // PrefProvider::PrefProvider.
  });
}

PrefProvider::~PrefProvider() {
  DCHECK(!prefs_);
}

std::unique_ptr<RuleIterator> PrefProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record,
    const PartitionKey& partition_key) const {
  if (!supports_type(content_type)) {
    return nullptr;
  }

  return GetPref(content_type)->GetRuleIterator(off_the_record, partition_key);
}

std::unique_ptr<Rule> PrefProvider::GetRule(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool off_the_record,
    const PartitionKey& partition_key) const {
  if (!supports_type(content_type)) {
    return nullptr;
  }

  return GetPref(content_type)
      ->GetRule(primary_url, secondary_url, off_the_record, partition_key);
}

// TODO(b/307193732): handle the PartitionKey in all relevant methods.
bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& in_value,
    const ContentSettingConstraints& constraints,
    const PartitionKey& partition_key) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  if (!supports_type(content_type)) {
    return false;
  }

  // Default settings are set using a wildcard pattern for both
  // |primary_pattern| and |secondary_pattern|. Don't store default settings in
  // the |PrefProvider|. The |PrefProvider| handles settings for specific
  // sites/origins defined by the |primary_pattern| and the |secondary_pattern|.
  // Default settings are handled by the |DefaultProvider|.
  if (primary_pattern == ContentSettingsPattern::Wildcard() &&
      secondary_pattern == ContentSettingsPattern::Wildcard()) {
    return false;
  }

  base::Time modified_time =
      store_last_modified_ ? clock_->Now() : base::Time();

  DCHECK(!constraints.track_last_visit_for_autoexpiration() ||
         content_settings::CanTrackLastVisit(content_type));
  // Last visit timestamps can only be tracked for host-specific pattern.
  DCHECK(!constraints.track_last_visit_for_autoexpiration() ||
         !primary_pattern.GetHost().empty());

  base::Time last_visited = constraints.track_last_visit_for_autoexpiration()
                                ? GetCoarseVisitedTime(clock_->Now())
                                : base::Time();

  // If mojom::SessionModel is ONE_TIME, we know for sure that a one time
  // permission has been set by the One Time Provider, therefore we reset a
  // potentially existing Allow Always setting.
  if (constraints.session_model() == mojom::SessionModel::ONE_TIME) {
    DCHECK(base::Contains(GetTypesWithTemporaryGrantsInHcsm(), content_type));
    in_value = base::Value();
  }

  RuleMetaData metadata;
  metadata.set_last_modified(modified_time);
  metadata.set_last_visited(last_visited);
  metadata.SetFromConstraints(constraints);
  GetPref(content_type)
      ->SetWebsiteSetting(primary_pattern, secondary_pattern,
                          std::move(in_value), metadata, partition_key);
  return true;
}

bool PrefProvider::SetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const base::Time time,
    const PartitionKey& partition_key) {
  return UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern == primary_pattern &&
               rule.secondary_pattern == secondary_pattern;
      },
      [&](Rule& rule) -> bool {
        // This should only be updated for settings that are
        // already tracked.
        DCHECK_NE(rule.metadata.last_visited(), base::Time());

        rule.metadata.set_last_visited(time);

        return true;
      },
      partition_key);
}

bool PrefProvider::UpdateSetting(ContentSettingsType content_type,
                                 base::FunctionRef<bool(const Rule&)> is_match,
                                 base::FunctionRef<bool(Rule&)> perform_update,
                                 const PartitionKey& partition_key) {
  if (!supports_type(content_type)) {
    return false;
  }

  auto it = GetRuleIterator(content_type, off_the_record_,
                            PartitionKey::WipGetDefault());
  if (!it) {
    return false;
  }

  while (it->HasNext()) {
    std::unique_ptr<Rule> rule = it->Next();
    if (!is_match(*rule)) {
      continue;
    }

    bool updated = perform_update(*rule);
    if (!updated) {
      return false;
    }
    base::Value value = std::move(rule->value);
    RuleMetaData metadata = std::move(rule->metadata);
    ContentSettingsPattern primary_pattern = std::move(rule->primary_pattern);
    ContentSettingsPattern secondary_pattern =
        std::move(rule->secondary_pattern);

    // Reset iterator and rule to release lock before updating setting.
    it.reset();
    rule.reset();

    GetPref(content_type)
        ->SetWebsiteSetting(std::move(primary_pattern),
                            std::move(secondary_pattern), std::move(value),
                            std::move(metadata), partition_key);
    return true;
  }
  return false;
}

bool PrefProvider::UpdateLastUsedTime(const GURL& primary_url,
                                      const GURL& secondary_url,
                                      ContentSettingsType content_type,
                                      const base::Time time,
                                      const PartitionKey& partition_key) {
  return UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern.Matches(primary_url) &&
               rule.secondary_pattern.Matches(secondary_url);
      },
      [&](Rule& rule) -> bool {
        rule.metadata.set_last_used(time);
        return true;
      },
      partition_key);
}

bool PrefProvider::ResetLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const PartitionKey& partition_key) {
  return SetLastVisitTime(primary_pattern, secondary_pattern, content_type,
                          base::Time(), partition_key);
}

bool PrefProvider::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const PartitionKey& partition_key) {
  return SetLastVisitTime(primary_pattern, secondary_pattern, content_type,
                          GetCoarseVisitedTime(clock_->Now()), partition_key);
}

std::optional<base::TimeDelta> PrefProvider::RenewContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    std::optional<ContentSetting> setting_to_match,
    const PartitionKey& partition_key) {
  std::optional<base::TimeDelta> delta_to_expiration;
  UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern.Matches(primary_url) &&
               rule.secondary_pattern.Matches(secondary_url) &&
               (!setting_to_match.has_value() ||
                setting_to_match.value() ==
                    content_settings::ValueToContentSetting(rule.value));
      },
      [&](Rule& rule) -> bool {
        // Only settings whose lifetimes are non-zero can be
        // renewed.
        if (rule.metadata.lifetime().is_zero()) {
          return false;
        }

        if (rule.metadata.expiration() < clock_->Now()) {
          return false;
        }

        base::TimeDelta lifetime = rule.metadata.lifetime();
        delta_to_expiration = rule.metadata.expiration() - clock_->Now();
        rule.metadata.SetExpirationAndLifetime(clock_->Now() + lifetime,
                                               lifetime);

        return true;
      },
      partition_key);
  return delta_to_expiration;
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type,
    const PartitionKey& partition_key) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  if (supports_type(content_type)) {
    GetPref(content_type)->ClearAllContentSettingsRules(partition_key);
  }
}

void PrefProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  for (const auto& pref : content_settings_prefs_) {
    pref.second->OnShutdown();
  }
  pref_change_registrar_.Reset();
  prefs_ = nullptr;
}

ContentSettingsPref* PrefProvider::GetPref(ContentSettingsType type) const {
  auto it = content_settings_prefs_.find(type);
  CHECK(it != content_settings_prefs_.end(), base::NotFatalUntil::M130);
  return it->second.get();
}

void PrefProvider::Notify(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type,
                          const PartitionKey* partition_key) {
  NotifyObservers(primary_pattern, secondary_pattern, content_type,
                  partition_key);
}

void PrefProvider::DiscardOrMigrateObsoletePreferences() {
  if (off_the_record_) {
    return;
  }

  // These prefs were never stored on iOS/Android so they don't need to be
  // deleted.
#if !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  prefs_->ClearPref(kObsoleteInstalledWebAppMetadataExceptionsPref);
  prefs_->ClearPref(kObsoletePpapiBrokerExceptionsPref);
#endif  // !BUILDFLAG(IS_IOS) && !BUILDFLAG(IS_ANDROID)
  prefs_->ClearPref(
      kObsoleteGetDisplayMediaSetAutoSelectAllScreensAllowedForUrlsExceptionsPref);
  prefs_->ClearPref(kObsoleteFederatedIdentityActiveSesssionExceptionsPref);

#if !BUILDFLAG(IS_IOS)
  // TODO(https://crbug.com/367181093): clean this up.
  if (!prefs_->GetBoolean(kBug364820109AlreadyWorkedAroundPref)) {
    prefs_->ClearPref(kBug364820109ExceptionSettingToClear);
    prefs_->SetBoolean(kBug364820109AlreadyWorkedAroundPref, true);
  }
#endif  // !BUILDFLAG(IS_IOS)
}

void PrefProvider::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
  for (auto& pref : content_settings_prefs_) {
    pref.second->SetClockForTesting(clock);  // IN-TEST
  }
}

}  // namespace content_settings
