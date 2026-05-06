// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_pref_provider.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
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
#include "components/content_settings/core/common/content_settings_enums.mojom-shared.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/features.h"
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
constexpr char kObsoletePrivateNetworkChooserDataPref[] =
    "profile.content_settings.exceptions.private_network_chooser_data";

constexpr char kGeolocationMigrateExceptionsPref[] =
    "profile.content_settings.exceptions.migrate_geolocation";

constexpr char kObsoleteTpcdHeuristicsGrantsPref[] =
    "profile.content_settings.exceptions.3pcd_heuristics_grants";

#if !BUILDFLAG(IS_IOS)
constexpr char kObsoleteTpcdTrialExceptionsPref[] =
    "profile.content_settings.exceptions.3pcd_support";
constexpr char kObsoleteTopLevelTpcdTrialExceptionsPref[] =
    "profile.content_settings.exceptions.top_level_3pcd_support";
constexpr char kObsoleteTopLevelTpcdOriginTrialExceptionsPref[] =
    "profile.content_settings.exceptions.top_level_3pcd_origin_trial";
// This setting was accidentally bound to a UI surface intended for a different
// setting (https://crbug.com/364820109). It should not have been settable
// except via enterprise policy, so it is temporarily cleaned up here to revert
// it to its default value.
// TODO(https://crbug.com/367181093): clean this up.
constexpr char kBug364820109AlreadyWorkedAroundPref[] =
    "profile.did_work_around_bug_364820109_exceptions";
constexpr char kLocalNetworkAccessMigrateExceptionsPref[] =
    "profile.content_settings.exceptions.has_migrated_local_network_access";
constexpr char kObsoleteTrackingProtectionExceptionsPref[] =
    "profile.content_settings.exceptions.tracking_protection";
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
  registry->RegisterBooleanPref(kGeolocationMigrateExceptionsPref, false);

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
  registry->RegisterDictionaryPref(kObsoletePrivateNetworkChooserDataPref);
  registry->RegisterDictionaryPref(kObsoleteTpcdHeuristicsGrantsPref);
#if !BUILDFLAG(IS_IOS)
  registry->RegisterDictionaryPref(kObsoleteTpcdTrialExceptionsPref);
  registry->RegisterDictionaryPref(kObsoleteTopLevelTpcdTrialExceptionsPref);
  registry->RegisterDictionaryPref(
      kObsoleteTopLevelTpcdOriginTrialExceptionsPref);
  registry->RegisterDictionaryPref(kObsoleteTrackingProtectionExceptionsPref);
  // TODO(https://crbug.com/367181093): clean this up.
  registry->RegisterBooleanPref(kBug364820109AlreadyWorkedAroundPref, false);
  registry->RegisterBooleanPref(kLocalNetworkAccessMigrateExceptionsPref,
                                false);
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
        info->type(), std::make_unique<ContentSettingsPref>(
                          info->type(), prefs_, &pref_change_registrar_,
                          info->pref_name(), off_the_record_, restore_session,
                          base::BindRepeating(&PrefProvider::Notify,
                                              base::Unretained(this)))));
  }

#if !BUILDFLAG(IS_IOS)
  MigrateGeolocationExceptions();
  MigrateLocalNetworkAccessExceptions();
#endif  // !BUILDFLAG(IS_IOS)

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
    bool off_the_record) const {
  if (!supports_type(content_type)) {
    return nullptr;
  }

  return GetPref(content_type)->GetRuleIterator(off_the_record);
}

std::unique_ptr<Rule> PrefProvider::GetRule(const GURL& primary_url,
                                            const GURL& secondary_url,
                                            ContentSettingsType content_type,
                                            bool off_the_record) const {
  if (!supports_type(content_type)) {
    return nullptr;
  }

  return GetPref(content_type)
      ->GetRule(primary_url, secondary_url, off_the_record);
}

bool PrefProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value&& in_value,
    const ContentSettingConstraints& constraints) {
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
         content_settings::CanTrackLastVisit(content_type))
      << content_type;
  // Last visit timestamps can only be tracked for host-specific pattern.
  DCHECK(!constraints.track_last_visit_for_autoexpiration() ||
         !primary_pattern.GetHost().empty());

  base::Time last_visited = constraints.track_last_visit_for_autoexpiration()
                                ? GetCoarseVisitedTime(clock_->Now())
                                : base::Time();

  RuleMetaData metadata;
  metadata.set_last_modified(modified_time);
  metadata.set_last_visited(last_visited);
  metadata.SetFromConstraints(constraints);

  // If mojom::SessionModel is ONE_TIME, we know for sure that a one time
  // permission has been set by the One Time Provider.
  if (constraints.session_model() == mojom::SessionModel::ONE_TIME) {
    DCHECK(std::ranges::contains(GetTypesWithTemporaryGrantsInHcsm(),
                                 content_type));

    mojom::SessionModel session_model;
    std::optional<base::Value> new_value = ValueForHandlingEphemeralGrant(
        primary_pattern, secondary_pattern, content_type, in_value, constraints,
        &session_model);
    if (new_value.has_value()) {
      in_value = std::move(new_value).value();
      metadata.set_session_model(session_model);
    } else {
      return true;
    }
  }

  GetPref(content_type)
      ->SetWebsiteSetting(primary_pattern, secondary_pattern,
                          std::move(in_value), std::move(metadata));
  return true;
}

std::optional<base::Value> PrefProvider::ValueForHandlingEphemeralGrant(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const base::Value& in_value,
    const ContentSettingConstraints& constraints,
    mojom::SessionModel* session_model) {
  // If we should clear the persistent grant, let's just do it.
  if (constraints.ephemeral_clears_persistent_grant()) {
    return base::Value();
  }

  // Otherwise reset potentially BLOCKED states to ASK.
  auto* info = content_settings::PermissionSettingsRegistry::GetInstance()->Get(
      content_type);
  CHECK(info);

  std::optional<PermissionSetting> setting =
      info->delegate().FromValue(in_value);
  if (!setting.has_value()) {
    return std::nullopt;
  }
  std::unique_ptr<Rule> current_rule =
      GetPref(content_type)
          ->GetRule(primary_pattern.ToRepresentativeUrl(),
                    secondary_pattern.ToRepresentativeUrl(), off_the_record_);
  if (!current_rule) {
    return std::nullopt;
  }
  CHECK_EQ(current_rule->primary_pattern, primary_pattern);
  CHECK_EQ(current_rule->secondary_pattern, secondary_pattern);
  std::optional<PermissionSetting> current_setting =
      info->delegate().FromValue(current_rule->value);
  *session_model = current_rule->metadata.session_model();
  if (!current_setting) {
    // Invalid setting, let's clean it up.
    return base::Value();
  }
  PermissionSetting new_setting =
      info->delegate().RemoveBlockedPermissionsForEphemeralGrant(
          *current_setting, *setting);
  if (new_setting != *current_setting) {
    return info->delegate().IsUndecided(new_setting)
               ? base::Value()
               : info->delegate().ToValue(new_setting);
  } else {
    return std::nullopt;
  }
}

bool PrefProvider::UpdateLastVisitTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  return UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern == primary_pattern &&
               rule.secondary_pattern == secondary_pattern;
      },
      [&](Rule& rule) -> bool {
        // TODO(crbug.com/40267370): Re-add the DCHECK to ensure the existing
        // `last_visited` is not null.
        rule.metadata.set_last_visited(GetCoarseVisitedTime(clock_->Now()));
        return true;
      });
}

bool PrefProvider::UpdateSetting(
    ContentSettingsType content_type,
    base::FunctionRef<bool(const Rule&)> is_match,
    base::FunctionRef<bool(Rule&)> perform_update) {
  if (!supports_type(content_type)) {
    return false;
  }

  auto it = GetRuleIterator(content_type, off_the_record_);
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
                            std::move(metadata));
    return true;
  }
  return false;
}

bool PrefProvider::UpdateLastUsedTime(const GURL& primary_url,
                                      const GURL& secondary_url,
                                      ContentSettingsType content_type,
                                      const base::Time time) {
  return UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern.Matches(primary_url) &&
               rule.secondary_pattern.Matches(secondary_url);
      },
      [&](Rule& rule) -> bool {
        rule.metadata.set_last_used(time);
        return true;
      });
}

bool PrefProvider::SetAutorevocationBypassedByUser(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  return UpdateSetting(
      content_type,
      [&](const Rule& rule) -> bool {
        return rule.primary_pattern == primary_pattern &&
               rule.secondary_pattern == secondary_pattern;
      },
      [&](Rule& rule) -> bool {
        rule.metadata.set_autorevocation_bypassed_by_user(true);
        return true;
      });
}

std::optional<base::TimeDelta> PrefProvider::RenewContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    std::optional<ContentSetting> setting_to_match) {
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
      });
  return delta_to_expiration;
}

void PrefProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  if (supports_type(content_type)) {
    GetPref(content_type)->ClearAllContentSettingsRules();
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
  CHECK(it != content_settings_prefs_.end());
  return it->second.get();
}

void PrefProvider::Notify(const ContentSettingsPattern& primary_pattern,
                          const ContentSettingsPattern& secondary_pattern,
                          ContentSettingsType content_type) {
  NotifyObservers(primary_pattern, secondary_pattern, content_type);
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
  prefs_->ClearPref(kObsoletePrivateNetworkChooserDataPref);
  prefs_->ClearPref(kObsoleteTpcdHeuristicsGrantsPref);

#if !BUILDFLAG(IS_IOS)
  prefs_->ClearPref(kObsoleteTpcdTrialExceptionsPref);
  prefs_->ClearPref(kObsoleteTopLevelTpcdTrialExceptionsPref);
  prefs_->ClearPref(kObsoleteTopLevelTpcdOriginTrialExceptionsPref);
  prefs_->ClearPref(kObsoleteTrackingProtectionExceptionsPref);
  // TODO(https://crbug.com/367181093): clean this up.
  prefs_->ClearPref(kBug364820109AlreadyWorkedAroundPref);
#endif  // !BUILDFLAG(IS_IOS)
}

#if !BUILDFLAG(IS_IOS)
void PrefProvider::MigrateGeolocationExceptions() {
  if (off_the_record_) {
    return;
  }

  auto* info = PermissionSettingsRegistry::GetInstance()->Get(
      ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
  // Migrate when the feature gets enabled the first time.
  if (base::FeatureList::IsEnabled(
          features::kApproximateGeolocationPermission) &&
      !prefs_->GetBoolean(kGeolocationMigrateExceptionsPref)) {
    auto* old_pref = GetPref(ContentSettingsType::GEOLOCATION);
    auto* options_pref = GetPref(ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
    auto it = old_pref->GetRuleIterator(false);
    while (it && it->HasNext()) {
      auto rule = it->Next();
      auto content_setting = ValueToContentSetting(rule->value);
      auto geolocation_setting =
          GeolocationSetting{ToPermissionOption(content_setting),
                             ToPermissionOption(content_setting)};
      options_pref->SetWebsiteSetting(
          rule->primary_pattern, rule->secondary_pattern,
          info->delegate().ToValue(geolocation_setting),
          std::move(rule->metadata));
    }
    it.reset();
    old_pref->ClearAllContentSettingsRules();
    prefs_->SetBoolean(kGeolocationMigrateExceptionsPref, true);
  }

  // Migrate back when the feature is disabled the first time.
  if (!base::FeatureList::IsEnabled(
          features::kApproximateGeolocationPermission) &&
      prefs_->GetBoolean(kGeolocationMigrateExceptionsPref)) {
    auto* old_pref = GetPref(ContentSettingsType::GEOLOCATION);
    auto* options_pref = GetPref(ContentSettingsType::GEOLOCATION_WITH_OPTIONS);
    auto it = options_pref->GetRuleIterator(false);
    while (it && it->HasNext()) {
      auto rule = it->Next();
      auto geolocation_setting = std::get<GeolocationSetting>(
          ValueToPermissionSetting(info, rule->value));
      old_pref->SetWebsiteSetting(
          rule->primary_pattern, rule->secondary_pattern,
          ContentSettingToValue(ToContentSetting(geolocation_setting.precise)),
          std::move(rule->metadata));
    }
    it.reset();
    options_pref->ClearAllContentSettingsRules();
    prefs_->SetBoolean(kGeolocationMigrateExceptionsPref, false);
  }
}

void PrefProvider::MigrateLocalNetworkAccessExceptions() {
  if (off_the_record_) {
    return;
  }
  // If LNA isn't turned on at all, don't try to migrate anything.
  if (!base::FeatureList::IsEnabled(
          network::features::kLocalNetworkAccessChecks)) {
    return;
  }

  // Migrate only once, if the pref is not set yet.
  // All exceptions get migrated to LOCAL_NETWORK, but only ALLOW exceptions get
  // migrated to LOOPBACK_NETWORK, as the old prompt language was biased towards
  // LOCAL_NETWORK.
  if (!prefs_->GetBoolean(kLocalNetworkAccessMigrateExceptionsPref)) {
    auto* old_pref = GetPref(ContentSettingsType::LOCAL_NETWORK_ACCESS);
    auto* local_pref = GetPref(ContentSettingsType::LOCAL_NETWORK);
    auto* loopback_pref = GetPref(ContentSettingsType::LOOPBACK_NETWORK);
    auto it = old_pref->GetRuleIterator(false);

    while (it && it->HasNext()) {
      auto rule = it->Next();
      auto content_setting = ValueToContentSetting(rule->value);
      local_pref->SetWebsiteSetting(
          rule->primary_pattern, rule->secondary_pattern,
          ContentSettingToValue(content_setting), rule->metadata.Clone());
      if (content_setting != ContentSetting::CONTENT_SETTING_BLOCK) {
        loopback_pref->SetWebsiteSetting(
            rule->primary_pattern, rule->secondary_pattern,
            ContentSettingToValue(content_setting), rule->metadata.Clone());
      }
    }
    it.reset();
    old_pref->ClearAllContentSettingsRules();
    prefs_->SetBoolean(kLocalNetworkAccessMigrateExceptionsPref, true);
  }
}
#endif  // !BUILDFLAG(IS_IOS)

void PrefProvider::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
  for (auto& pref : content_settings_prefs_) {
    pref.second->SetClockForTesting(clock);  // IN-TEST
  }
}

}  // namespace content_settings
