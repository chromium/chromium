// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/host_content_settings_map.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_default_provider.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_observable_provider.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_policy_provider.h"
#include "components/content_settings/core/browser/content_settings_pref_provider.h"
#include "components/content_settings/core/browser/content_settings_provider.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_uma_util.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/base/net_errors.h"
#include "net/cookies/static_cookie_policy.h"
#include "url/gurl.h"

using content_settings::ContentSettingsInfo;
using content_settings::WebsiteSettingsInfo;

namespace {

typedef std::vector<content_settings::Rule> Rules;

typedef std::pair<std::string, std::string> StringPair;

struct ProviderNamesSourceMapEntry {
  const char* provider_name;
  content_settings::SettingSource provider_source;
};

const HostContentSettingsMap::ProviderType kFirstProvider =
    HostContentSettingsMap::WEBUI_ALLOWLIST_PROVIDER;
const HostContentSettingsMap::ProviderType kFirstUserModifiableProvider =
    HostContentSettingsMap::NOTIFICATION_ANDROID_PROVIDER;

constexpr ProviderNamesSourceMapEntry kProviderNamesSourceMap[] = {
    {"webui_allowlist", content_settings::SETTING_SOURCE_ALLOWLIST},
    {"policy", content_settings::SETTING_SOURCE_POLICY},
    {"supervised_user", content_settings::SETTING_SOURCE_SUPERVISED},
    {"extension", content_settings::SETTING_SOURCE_EXTENSION},
    {"installed_webapp_provider",
     content_settings::SETTING_SOURCE_INSTALLED_WEBAPP},
    {"notification_android", content_settings::SETTING_SOURCE_USER},
    {"one_time", content_settings::SETTING_SOURCE_USER},
    {"preference", content_settings::SETTING_SOURCE_USER},
    {"default", content_settings::SETTING_SOURCE_USER},
    {"tests", content_settings::SETTING_SOURCE_USER},
    {"tests_other", content_settings::SETTING_SOURCE_USER},
};

static_assert(
    std::size(kProviderNamesSourceMap) ==
        HostContentSettingsMap::NUM_PROVIDER_TYPES,
    "kProviderNamesSourceMap should have NUM_PROVIDER_TYPES elements");

// Ensure that kFirstUserModifiableProvider is actually the highest precedence
// user modifiable provider.
constexpr bool FirstUserModifiableProviderIsHighestPrecedence() {
  for (size_t i = 0; i < kFirstUserModifiableProvider; ++i) {
    if (kProviderNamesSourceMap[i].provider_source ==
        content_settings::SETTING_SOURCE_USER) {
      return false;
    }
  }
  return kProviderNamesSourceMap[kFirstUserModifiableProvider]
             .provider_source == content_settings::SETTING_SOURCE_USER;
}
static_assert(FirstUserModifiableProviderIsHighestPrecedence(),
              "kFirstUserModifiableProvider is not the highest precedence user "
              "modifiable provider.");

bool SchemeCanBeAllowlisted(const std::string& scheme) {
  return scheme == content_settings::kChromeDevToolsScheme ||
         scheme == content_settings::kExtensionScheme ||
         scheme == content_settings::kChromeUIScheme;
}

// Handles inheritance of settings from the regular profile into the incognito
// profile.
base::Value ProcessIncognitoInheritanceBehavior(
    ContentSettingsType content_type,
    base::Value value) {
  // Website setting inheritance can be completely disallowed.
  const WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(
          content_type);
  if (website_settings_info &&
      website_settings_info->incognito_behavior() ==
          WebsiteSettingsInfo::DONT_INHERIT_IN_INCOGNITO) {
    return base::Value();
  }

  // Content setting inheritance can be for settings, that are more permissive
  // than the initial value of a content setting.
  const ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    ContentSettingsInfo::IncognitoBehavior behaviour =
        content_settings_info->incognito_behavior();
    switch (behaviour) {
      case ContentSettingsInfo::INHERIT_IN_INCOGNITO:
        return value;
      case ContentSettingsInfo::INHERIT_IF_LESS_PERMISSIVE:
        ContentSetting setting = content_settings::ValueToContentSetting(value);
        const base::Value& initial_value =
            content_settings_info->website_settings_info()
                ->initial_default_value();
        ContentSetting initial_setting =
            content_settings::ValueToContentSetting(initial_value);
        if (content_settings::IsMorePermissive(setting, initial_setting))
          return content_settings::ContentSettingToValue(initial_setting);
        return value;
    }
  }

  return value;
}

content_settings::PatternPair GetPatternsFromScopingType(
    WebsiteSettingsInfo::ScopingType scoping_type,
    const GURL& primary_url,
    const GURL& secondary_url) {
  DCHECK(!primary_url.is_empty());
  content_settings::PatternPair patterns;

  switch (scoping_type) {
    case WebsiteSettingsInfo::
        REQUESTING_ORIGIN_WITH_TOP_ORIGIN_EXCEPTIONS_SCOPE:
      patterns.first = ContentSettingsPattern::FromURL(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::REQUESTING_AND_TOP_ORIGIN_SCOPE:
      CHECK(!secondary_url.is_empty());
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second =
          ContentSettingsPattern::FromURLNoWildcard(secondary_url);
      break;
    case WebsiteSettingsInfo::REQUESTING_AND_TOP_SCHEMEFUL_SITE_SCOPE:
      CHECK(!secondary_url.is_empty());
      patterns.first =
          ContentSettingsPattern::FromURLToSchemefulSitePattern(primary_url);
      patterns.second =
          ContentSettingsPattern::FromURLToSchemefulSitePattern(secondary_url);
      break;
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_AND_TOP_SCHEMEFUL_SITE_SCOPE:
      CHECK(!secondary_url.is_empty());
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second =
          ContentSettingsPattern::FromURLToSchemefulSitePattern(secondary_url);
      break;
    case WebsiteSettingsInfo::REQUESTING_SCHEMEFUL_SITE_ONLY_SCOPE:
      patterns.first =
          ContentSettingsPattern::FromURLToSchemefulSitePattern(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
    case WebsiteSettingsInfo::TOP_ORIGIN_ONLY_SCOPE:
    case WebsiteSettingsInfo::REQUESTING_ORIGIN_ONLY_SCOPE:
    case WebsiteSettingsInfo::GENERIC_SINGLE_ORIGIN_SCOPE:
    case WebsiteSettingsInfo::TOP_ORIGIN_WITH_RESOURCE_EXCEPTIONS_SCOPE:
      patterns.first = ContentSettingsPattern::FromURLNoWildcard(primary_url);
      patterns.second = ContentSettingsPattern::Wildcard();
      break;
  }
  return patterns;
}

content_settings::PatternPair GetPatternsForContentSettingsType(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) {
  const WebsiteSettingsInfo* website_settings_info =
      content_settings::WebsiteSettingsRegistry::GetInstance()->Get(type);
  DCHECK(website_settings_info);
  content_settings::PatternPair patterns = GetPatternsFromScopingType(
      website_settings_info->scoping_type(), primary_url, secondary_url);
  return patterns;
}

// This enum is used to collect Flash permission data.
enum class FlashPermissions {
  kFirstTime = 0,
  kRepeated = 1,
  kMaxValue = kRepeated,
};

// Returns whether per-content setting exception information should be
// collected. All content settings for which this method returns true here be
// content settings, not website settings (i.e. their value should be a
// ContentSetting).
//
// This method should be kept in sync with histograms.xml, as every type here
// is an affected histogram under the "ContentSetting" suffix.
bool ShouldCollectFineGrainedExceptionHistograms(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::COOKIES:
    case ContentSettingsType::POPUPS:
    case ContentSettingsType::ADS:
    case ContentSettingsType::STORAGE_ACCESS:
      return true;
    default:
      return false;
  }
}

// Returns whether information about the maximum number of exceptions per
// embedder/requester should be recorded. Only relevant for setting types that
// are keyed to both requester and embedder.
bool ShouldCollectRequesterAndEmbedderHistograms(ContentSettingsType type) {
  switch (type) {
    case ContentSettingsType::STORAGE_ACCESS:
      return true;
    default:
      return false;
  }
}

const char* ContentSettingToString(ContentSetting setting) {
  switch (setting) {
    case CONTENT_SETTING_ALLOW:
      return "Allow";
    case CONTENT_SETTING_BLOCK:
      return "Block";
    case CONTENT_SETTING_ASK:
      return "Ask";
    case CONTENT_SETTING_SESSION_ONLY:
      return "SessionOnly";
    case CONTENT_SETTING_DETECT_IMPORTANT_CONTENT:
      return "DetectImportantContent";
    case CONTENT_SETTING_DEFAULT:
    case CONTENT_SETTING_NUM_SETTINGS:
      NOTREACHED();
      return nullptr;
  }
}

struct ContentSettingEntry {
  ContentSettingsType type;
  ContentSettingsPattern primary_pattern;
  ContentSettingsPattern secondary_pattern;
};

}  // namespace

HostContentSettingsMap::HostContentSettingsMap(PrefService* prefs,
                                               bool is_off_the_record,
                                               bool store_last_modified,
                                               bool restore_session,
                                               bool should_record_metrics)
    : RefcountedKeyedService(base::SingleThreadTaskRunner::GetCurrentDefault()),
#ifndef NDEBUG
      used_from_thread_id_(base::PlatformThread::CurrentId()),
#endif
      prefs_(prefs),
      is_off_the_record_(is_off_the_record),
      store_last_modified_(store_last_modified),
      allow_invalid_secondary_pattern_for_testing_(false),
      clock_(base::DefaultClock::GetInstance()) {
  TRACE_EVENT0("startup", "HostContentSettingsMap::HostContentSettingsMap");

  auto policy_provider_ptr =
      std::make_unique<content_settings::PolicyProvider>(prefs_);
  auto* policy_provider = policy_provider_ptr.get();
  content_settings_providers_[POLICY_PROVIDER] = std::move(policy_provider_ptr);
  policy_provider->AddObserver(this);

  auto pref_provider_ptr = std::make_unique<content_settings::PrefProvider>(
      prefs_, is_off_the_record_, store_last_modified_, restore_session);
  pref_provider_ = pref_provider_ptr.get();
  content_settings_providers_[PREF_PROVIDER] = std::move(pref_provider_ptr);
  user_modifiable_providers_.push_back(pref_provider_);
  pref_provider_->AddObserver(this);

  auto default_provider = std::make_unique<content_settings::DefaultProvider>(
      prefs_, is_off_the_record_, should_record_metrics);
  default_provider->AddObserver(this);
  content_settings_providers_[DEFAULT_PROVIDER] = std::move(default_provider);

  MigrateSettingsPrecedingPermissionDelegationActivation();
  if (should_record_metrics) {
    RecordExceptionMetrics();
  }

  if (base::FeatureList::IsEnabled(
          content_settings::features::kActiveContentSettingExpiry)) {
    const auto* registry =
        content_settings::WebsiteSettingsRegistry::GetInstance();
    for (const auto* info : *registry) {
      DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(info->type());
    }
  }
}

// static
void HostContentSettingsMap::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Ensure the content settings are all registered.
  content_settings::ContentSettingsRegistry::GetInstance();

  registry->RegisterIntegerPref(prefs::kContentSettingsWindowLastTabIndex, 0);

  // Register the prefs for the content settings providers.
  content_settings::DefaultProvider::RegisterProfilePrefs(registry);
  content_settings::PrefProvider::RegisterProfilePrefs(registry);
  content_settings::PolicyProvider::RegisterProfilePrefs(registry);
}

void HostContentSettingsMap::RegisterUserModifiableProvider(
    ProviderType type,
    std::unique_ptr<content_settings::UserModifiableProvider> provider) {
  user_modifiable_providers_.push_back(provider.get());
  RegisterProvider(type, std::move(provider));
}

void HostContentSettingsMap::RegisterProvider(
    ProviderType type,
    std::unique_ptr<content_settings::ObservableProvider> provider) {
  DCHECK(!content_settings_providers_[type]);
  provider->AddObserver(this);
  content_settings_providers_[type] = std::move(provider);

#ifndef NDEBUG
  DCHECK_NE(used_from_thread_id_, base::kInvalidThreadId)
      << "Used from multiple threads before initialization complete.";
#endif

  OnContentSettingChanged(ContentSettingsPattern::Wildcard(),
                          ContentSettingsPattern::Wildcard(),
                          ContentSettingsTypeSet::AllTypes());
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingFromProvider(
    ContentSettingsType content_type,
    content_settings::ProviderInterface* provider) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, false));

  if (rule_iterator) {
    ContentSettingsPattern wildcard = ContentSettingsPattern::Wildcard();
    while (rule_iterator->HasNext()) {
      std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
      if (rule->primary_pattern == wildcard &&
          rule->secondary_pattern == wildcard) {
        return content_settings::ValueToContentSetting(rule->value());
      }
    }
  }
  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSettingInternal(
    ContentSettingsType content_type,
    ProviderType* provider_type) const {
  DCHECK(provider_type);
  UsedContentSettingsProviders();

  // Iterate through the list of providers and return the first non-NULL value
  // that matches |primary_url| and |secondary_url|.
  for (const auto& provider_pair : content_settings_providers_) {
    if (provider_pair.first == PREF_PROVIDER)
      continue;
    ContentSetting default_setting = GetDefaultContentSettingFromProvider(
        content_type, provider_pair.second.get());
    if (is_off_the_record_) {
      default_setting = content_settings::ValueToContentSetting(
          ProcessIncognitoInheritanceBehavior(
              content_type,
              content_settings::ContentSettingToValue(default_setting)));
    }
    if (default_setting != CONTENT_SETTING_DEFAULT) {
      *provider_type = provider_pair.first;
      return default_setting;
    }
  }

  return CONTENT_SETTING_DEFAULT;
}

ContentSetting HostContentSettingsMap::GetDefaultContentSetting(
    ContentSettingsType content_type,
    std::string* provider_id) const {
  ProviderType provider_type = NUM_PROVIDER_TYPES;
  ContentSetting content_setting =
      GetDefaultContentSettingInternal(content_type, &provider_type);
  if (content_setting != CONTENT_SETTING_DEFAULT && provider_id)
    *provider_id = kProviderNamesSourceMap[provider_type].provider_name;
  return content_setting;
}

ContentSetting HostContentSettingsMap::GetContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  const base::Value value =
      GetWebsiteSetting(primary_url, secondary_url, content_type, info);
  return content_settings::ValueToContentSetting(value);
}

ContentSetting HostContentSettingsMap::GetUserModifiableContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type) const {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));
  const base::Value value =
      GetWebsiteSettingInternal(primary_url, secondary_url, content_type,
                                kFirstUserModifiableProvider, nullptr);
  return content_settings::ValueToContentSetting(value);
}

ContentSettingsForOneType HostContentSettingsMap::GetSettingsForOneType(
    ContentSettingsType content_type,
    absl::optional<content_settings::SessionModel> session_model) const {
  ContentSettingsForOneType settings;
  UsedContentSettingsProviders();

  for (const auto& provider_pair : content_settings_providers_) {
    // For each provider, iterate first the incognito-specific rules, then the
    // normal rules.
    if (is_off_the_record_) {
      AddSettingsForOneType(provider_pair.second.get(), provider_pair.first,
                            content_type, &settings, true, session_model);
    }
    AddSettingsForOneType(provider_pair.second.get(), provider_pair.first,
                          content_type, &settings, false, session_model);
  }
  return settings;
}

void HostContentSettingsMap::SetDefaultContentSetting(
    ContentSettingsType content_type,
    ContentSetting setting) {
  base::Value value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(content_settings::ContentSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->IsDefaultSettingValid(setting));
    value = base::Value(setting);
  }
  SetWebsiteSettingCustomScope(ContentSettingsPattern::Wildcard(),
                               ContentSettingsPattern::Wildcard(), content_type,
                               std::move(value));
}

void HostContentSettingsMap::SetWebsiteSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    base::Value value,
    const content_settings::ContentSettingConstraints& constraints) {
  content_settings::PatternPair patterns = GetPatternsForContentSettingsType(
      primary_url, secondary_url, content_type);
  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               std::move(value), constraints);
}

void HostContentSettingsMap::SetWebsiteSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    base::Value value,
    const content_settings::ContentSettingConstraints& constraints) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(IsSecondaryPatternAllowed(primary_pattern, secondary_pattern,
                                   content_type, value));
  // TODO(crbug.com/731126): Verify that assumptions for notification content
  // settings are met.
  UsedContentSettingsProviders();

#if DCHECK_IS_ON()
  base::Value clone = value.Clone();
#endif
  for (const auto& provider_pair : content_settings_providers_) {
    // The std::move(value) here just turns the value into an r-value reference.
    // It doesn't actually move the value yet. The provider can decide to accept
    // the value. If successful then ownership is passed to the provider.
    if (provider_pair.second->SetWebsiteSetting(
            primary_pattern, secondary_pattern, content_type, std::move(value),
            constraints)) {
      if (base::FeatureList::IsEnabled(
              content_settings::features::kActiveContentSettingExpiry)) {
        UpdateExpiryEnforcementTimer(content_type, constraints.expiration());
      }

      return;
    }

    // Ensure that the value is unmodified until accepted by a provider.
#if DCHECK_IS_ON()
    DCHECK_EQ(value, clone);
#endif
  }
  NOTREACHED();
}

bool HostContentSettingsMap::CanSetNarrowestContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) const {
  content_settings::PatternPair patterns =
      GetNarrowestPatterns(primary_url, secondary_url, type);
  return patterns.first.IsValid() && patterns.second.IsValid();
}

bool HostContentSettingsMap::IsRestrictedToSecureOrigins(
    ContentSettingsType type) const {
  const ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(type);
  DCHECK(content_settings_info);

  return content_settings_info->origin_restriction() ==
         ContentSettingsInfo::EXCEPTIONS_ON_SECURE_ORIGINS_ONLY;
}

void HostContentSettingsMap::SetNarrowestContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    ContentSetting setting,
    const content_settings::ContentSettingConstraints& constraints) {
  content_settings::PatternPair patterns =
      GetNarrowestPatterns(primary_url, secondary_url, type);

  if (!patterns.first.IsValid() || !patterns.second.IsValid())
    return;

  SetContentSettingCustomScope(patterns.first, patterns.second, type, setting,
                               constraints);
}

content_settings::PatternPair HostContentSettingsMap::GetNarrowestPatterns(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type) const {
  // Permission settings are specified via rules. There exists always at least
  // one rule for the default setting. Get the rule that currently defines
  // the permission for the given permission |type|. Then test whether the
  // existing rule is more specific than the rule we are about to create. If
  // the existing rule is more specific, than change the existing rule instead
  // of creating a new rule that would be hidden behind the existing rule->
  content_settings::SettingInfo info;
  GetWebsiteSettingInternal(primary_url, secondary_url, type, kFirstProvider,
                            &info);
  if (info.source != content_settings::SETTING_SOURCE_USER) {
    // Return an invalid pattern if the current setting is not a user setting
    // and thus can't be changed.
    return content_settings::PatternPair();
  }

  content_settings::PatternPair patterns =
      GetPatternsForContentSettingsType(primary_url, secondary_url, type);

  ContentSettingsPattern::Relation r1 =
      info.primary_pattern.Compare(patterns.first);
  if (r1 == ContentSettingsPattern::PREDECESSOR) {
    patterns.first = std::move(info.primary_pattern);
  } else if (r1 == ContentSettingsPattern::IDENTITY) {
    ContentSettingsPattern::Relation r2 =
        info.secondary_pattern.Compare(patterns.second);
    DCHECK(r2 != ContentSettingsPattern::DISJOINT_ORDER_POST &&
           r2 != ContentSettingsPattern::DISJOINT_ORDER_PRE);
    if (r2 == ContentSettingsPattern::PREDECESSOR)
      patterns.second = std::move(info.secondary_pattern);
  }

  return patterns;
}

void HostContentSettingsMap::SetContentSettingCustomScope(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    ContentSetting setting,
    const content_settings::ContentSettingConstraints& constraints) {
  DCHECK(content_settings::ContentSettingsRegistry::GetInstance()->Get(
      content_type));

  base::Value value;
  // A value of CONTENT_SETTING_DEFAULT implies deleting the content setting.
  if (setting != CONTENT_SETTING_DEFAULT) {
    DCHECK(content_settings::ContentSettingsRegistry::GetInstance()
               ->Get(content_type)
               ->IsSettingValid(setting));
    value = base::Value(setting);
  }
  SetWebsiteSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               std::move(value), constraints);
}

void HostContentSettingsMap::SetContentSettingDefaultScope(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    ContentSetting setting,
    const content_settings::ContentSettingConstraints& constraints) {
  content_settings::PatternPair patterns = GetPatternsForContentSettingsType(
      primary_url, secondary_url, content_type);

  ContentSettingsPattern primary_pattern = patterns.first;
  ContentSettingsPattern secondary_pattern = patterns.second;
  if (!primary_pattern.IsValid() || !secondary_pattern.IsValid())
    return;

  SetContentSettingCustomScope(primary_pattern, secondary_pattern, content_type,
                               setting, constraints);
}

base::WeakPtr<HostContentSettingsMap> HostContentSettingsMap::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void HostContentSettingsMap::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
  for (auto* provider : user_modifiable_providers_)
    provider->SetClockForTesting(clock);
}

void HostContentSettingsMap::RecordExceptionMetrics() {
  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info :
       *content_settings::WebsiteSettingsRegistry::GetInstance()) {
    ContentSettingsType content_type = info->type();
    const std::string type_name = info->name();

    size_t num_exceptions = 0;
    base::flat_map<ContentSetting, size_t> num_exceptions_with_setting;
    const content_settings::ContentSettingsInfo* content_info =
        content_setting_registry->Get(content_type);
    ContentSettingsForOneType settings = GetSettingsForOneType(content_type);
    for (const ContentSettingPatternSource& setting_entry : settings) {
      // Skip default settings.
      if (setting_entry.primary_pattern == ContentSettingsPattern::Wildcard() &&
          setting_entry.secondary_pattern ==
              ContentSettingsPattern::Wildcard()) {
        continue;
      }

      if (setting_entry.source == "preference") {
        // |content_info| will be non-nullptr iff |content_type| is a content
        // setting rather than a website setting.
        if (content_info)
          ++num_exceptions_with_setting[setting_entry.GetContentSetting()];
        ++num_exceptions;
      }
    }

    std::string histogram_name =
        "ContentSettings.RegularProfile.Exceptions." + type_name;
    base::UmaHistogramCustomCounts(histogram_name, num_exceptions, 1, 1000, 30);

    // For some ContentSettingTypes, collect exception histograms broken out by
    // ContentSetting.
    if (ShouldCollectFineGrainedExceptionHistograms(content_type)) {
      DCHECK(content_info);
      for (int setting = 0; setting < CONTENT_SETTING_NUM_SETTINGS; ++setting) {
        ContentSetting content_setting = IntToContentSetting(setting);
        if (!content_info->IsSettingValid(content_setting))
          continue;
        std::string histogram_with_suffix =
            histogram_name + "." + ContentSettingToString(content_setting);
        base::UmaHistogramCustomCounts(
            histogram_with_suffix, num_exceptions_with_setting[content_setting],
            1, 1000, 30);
      }
    }
    if (ShouldCollectRequesterAndEmbedderHistograms(content_type)) {
      std::map<ContentSettingsPattern, int> num_requester;
      std::map<ContentSettingsPattern, int> num_toplevel;
      for (const ContentSettingPatternSource& setting : settings) {
        if (setting.primary_pattern == ContentSettingsPattern::Wildcard() &&
            setting.secondary_pattern == ContentSettingsPattern::Wildcard()) {
          continue;
        }
        num_requester[setting.primary_pattern]++;
        num_toplevel[setting.secondary_pattern]++;
      }
      auto get_value = [](const std::pair<ContentSettingsPattern, int>& p) {
        return p.second;
      };
      bool empty = num_requester.empty();
      int max_requester =
          empty ? 0 : base::ranges::max(num_requester, {}, get_value).second;
      base::UmaHistogramCounts1000(histogram_name + ".MaxRequester",
                                   max_requester);
      int max_toplevel =
          empty ? 0 : base::ranges::max(num_toplevel, {}, get_value).second;
      base::UmaHistogramCounts1000(histogram_name + ".MaxTopLevel",
                                   max_toplevel);
    }
    if (content_type == ContentSettingsType::COOKIES) {
      RecordThirdPartyCookieMetrics(settings);
    }
  }
}

void HostContentSettingsMap::RecordThirdPartyCookieMetrics(
    const ContentSettingsForOneType& settings) {
  size_t num_3pc_allow_exceptions = 0;
  size_t num_3pc_allow_exceptions_temporary = 0;
  size_t num_3pc_allow_exceptions_domain_wildcard = 0;

  for (const ContentSettingPatternSource& setting_entry : settings) {
    if (setting_entry.source == "preference" &&
        setting_entry.primary_pattern.MatchesAllHosts() &&
        !setting_entry.secondary_pattern.MatchesAllHosts() &&
        setting_entry.GetContentSetting() == CONTENT_SETTING_ALLOW) {
      num_3pc_allow_exceptions++;
      if (!setting_entry.metadata.expiration().is_null()) {
        num_3pc_allow_exceptions_temporary++;
      }
      if (setting_entry.secondary_pattern.HasDomainWildcard()) {
        num_3pc_allow_exceptions_domain_wildcard++;
      }
    }
  }
  base::UmaHistogramCustomCounts(
      "ContentSettings.RegularProfile.Exceptions.cookies.AllowThirdParty",
      num_3pc_allow_exceptions, 1, 1000, 30);
  base::UmaHistogramCustomCounts(
      "ContentSettings.RegularProfile.Exceptions.cookies."
      "TemporaryAllowThirdParty",
      num_3pc_allow_exceptions_temporary, 1, 100, 20);
  base::UmaHistogramCustomCounts(
      "ContentSettings.RegularProfile.Exceptions.cookies."
      "DomainWildcardAllowThirdParty",
      num_3pc_allow_exceptions_domain_wildcard, 1, 100, 10);
}

void HostContentSettingsMap::AddObserver(content_settings::Observer* observer) {
  observers_.AddObserver(observer);
}

void HostContentSettingsMap::RemoveObserver(
    content_settings::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HostContentSettingsMap::FlushLossyWebsiteSettings() {
  prefs_->SchedulePendingLossyWrites();
}

void HostContentSettingsMap::UpdateLastUsedTime(const GURL& primary_url,
                                                const GURL& secondary_url,
                                                ContentSettingsType type,
                                                const base::Time time) {
  for (auto* provider : user_modifiable_providers_) {
    provider->UpdateLastUsedTime(primary_url, secondary_url, type, time);
  }
}

void HostContentSettingsMap::ResetLastVisitedTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type) {
  for (auto* provider : user_modifiable_providers_) {
    provider->ResetLastVisitTime(primary_pattern, secondary_pattern, type);
  }
}

void HostContentSettingsMap::UpdateLastVisitedTime(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType type) {
  for (auto* provider : user_modifiable_providers_) {
    provider->UpdateLastVisitTime(primary_pattern, secondary_pattern, type);
  }
}

absl::optional<base::TimeDelta> HostContentSettingsMap::RenewContentSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType type,
    absl::optional<ContentSetting> setting_to_match) {
  absl::optional<base::TimeDelta> delta_to_nearest_expiration = absl::nullopt;
  for (auto* provider : user_modifiable_providers_) {
    absl::optional<base::TimeDelta> delta_to_expiration =
        provider->RenewContentSetting(primary_url, secondary_url, type,
                                      setting_to_match);

    if (!delta_to_nearest_expiration.has_value()) {
      delta_to_nearest_expiration = delta_to_expiration;
    } else if (delta_to_expiration.has_value()) {
      delta_to_nearest_expiration =
          std::min(delta_to_nearest_expiration, delta_to_expiration);
    }
  }
  return delta_to_nearest_expiration;
}

void HostContentSettingsMap::ClearSettingsForOneType(
    ContentSettingsType content_type) {
  UsedContentSettingsProviders();
  for (const auto& provider_pair : content_settings_providers_)
    provider_pair.second->ClearAllContentSettingsRules(content_type);
  FlushLossyWebsiteSettings();
}

void HostContentSettingsMap::ClearSettingsForOneTypeWithPredicate(
    ContentSettingsType content_type,
    base::Time begin_time,
    base::Time end_time,
    PatternSourcePredicate pattern_predicate) {
  if (pattern_predicate.is_null() && begin_time.is_null() &&
      (end_time.is_null() || end_time.is_max())) {
    ClearSettingsForOneType(content_type);
    return;
  }
  ClearSettingsForOneTypeWithPredicate(
      content_type, [&](const ContentSettingPatternSource& setting) -> bool {
        if (!pattern_predicate.is_null() &&
            !pattern_predicate.Run(setting.primary_pattern,
                                   setting.secondary_pattern)) {
          return false;
        }
        base::Time last_modified = setting.metadata.last_modified();
        return last_modified >= begin_time &&
               (last_modified < end_time || end_time.is_null());
      });
}

void HostContentSettingsMap::ClearSettingsForOneTypeWithPredicate(
    ContentSettingsType content_type,
    base::FunctionRef<bool(const ContentSettingPatternSource&)> predicate) {
  UsedContentSettingsProviders();
  for (const ContentSettingPatternSource& setting :
       GetSettingsForOneType(content_type)) {
    if (predicate(setting)) {
      for (auto* provider : user_modifiable_providers_) {
        provider->SetWebsiteSetting(setting.primary_pattern,
                                    setting.secondary_pattern, content_type,
                                    base::Value(), {});
      }
    }
  }
}

void HostContentSettingsMap::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsTypeSet content_type_set) {
  DCHECK(primary_pattern.IsValid());
  DCHECK(secondary_pattern.IsValid());
  for (content_settings::Observer& observer : observers_) {
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     content_type_set);
    observer.OnContentSettingChanged(primary_pattern, secondary_pattern,
                                     content_type_set.GetTypeOrDefault());
  }
}

HostContentSettingsMap::~HostContentSettingsMap() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!prefs_);
}

void HostContentSettingsMap::ShutdownOnUIThread() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs_);
  expiration_enforcement_timers_.clear();
  prefs_ = nullptr;
  for (const auto& provider_pair : content_settings_providers_)
    provider_pair.second->ShutdownOnUIThread();
}

void HostContentSettingsMap::AddSettingsForOneType(
    const content_settings::ProviderInterface* provider,
    ProviderType provider_type,
    ContentSettingsType content_type,
    ContentSettingsForOneType* settings,
    bool incognito,
    absl::optional<content_settings::SessionModel> session_model) const {
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, incognito));
  if (!rule_iterator)
    return;

  while (rule_iterator->HasNext()) {
    std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
    base::Value value = rule->TakeValue();

    // We may be adding settings for only specific rule types. If that's the
    // case and this setting isn't a match, don't add it.
    if (session_model && (session_model != rule->metadata.session_model())) {
      continue;
    }

    // Unless settings are actively monitored and removed on expiry, we will
    // also avoid adding any expired rules. Note, that this may lead to an
    // inconsistent state, where observers aren't notified of the expiry of the
    // grant, but the grant is no longer provided. Also refer to the comment
    // near the definition of `kEagerExpiryBuffer` regarding provisioning and
    // CPU contention.
    if (!base::FeatureList::IsEnabled(
            content_settings::features::kActiveContentSettingExpiry)) {
      if ((!rule->metadata.expiration().is_null() &&
           (rule->metadata.expiration() < clock_->Now()))) {
        continue;
      }
    }

    // Normal rules applied to incognito profiles are subject to inheritance
    // settings.
    if (!incognito && is_off_the_record_) {
      base::Value inherit_value =
          ProcessIncognitoInheritanceBehavior(content_type, std::move(value));
      if (!inherit_value.is_none()) {
        value = std::move(inherit_value);
      } else {
        continue;
      }
    }
    settings->emplace_back(rule->primary_pattern, rule->secondary_pattern,
                           std::move(value),
                           kProviderNamesSourceMap[provider_type].provider_name,
                           incognito, rule->metadata);
  }
}

void HostContentSettingsMap::UsedContentSettingsProviders() const {
#ifndef NDEBUG
  if (used_from_thread_id_ == base::kInvalidThreadId)
    return;

  if (base::PlatformThread::CurrentId() != used_from_thread_id_)
    used_from_thread_id_ = base::kInvalidThreadId;
#endif
}

base::Value HostContentSettingsMap::GetWebsiteSetting(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    content_settings::SettingInfo* info) const {
  // Check if the requested setting is allowlisted.
  // TODO(raymes): Move this into GetContentSetting. This has nothing to do with
  // website settings
  const content_settings::ContentSettingsInfo* content_settings_info =
      content_settings::ContentSettingsRegistry::GetInstance()->Get(
          content_type);
  if (content_settings_info) {
    for (const std::string& scheme :
         content_settings_info->allowlisted_primary_schemes()) {
      DCHECK(SchemeCanBeAllowlisted(scheme));

      if (primary_url.SchemeIs(scheme)) {
        if (info) {
          info->source = content_settings::SETTING_SOURCE_ALLOWLIST;
          info->primary_pattern = ContentSettingsPattern::Wildcard();
          info->secondary_pattern = ContentSettingsPattern::Wildcard();
        }
        return base::Value(CONTENT_SETTING_ALLOW);
      }
    }
  }

  return GetWebsiteSettingInternal(primary_url, secondary_url, content_type,
                                   kFirstProvider, info);
}

// static
HostContentSettingsMap::ProviderType
HostContentSettingsMap::GetProviderTypeFromSource(const std::string& source) {
  for (size_t i = 0; i < std::size(kProviderNamesSourceMap); ++i) {
    if (source == kProviderNamesSourceMap[i].provider_name)
      return static_cast<ProviderType>(i);
  }

  NOTREACHED();
  return DEFAULT_PROVIDER;
}

// static
content_settings::SettingSource
HostContentSettingsMap::GetSettingSourceFromProviderName(
    const std::string& provider_name) {
  for (const auto& provider_name_source : kProviderNamesSourceMap) {
    if (provider_name == provider_name_source.provider_name)
      return provider_name_source.provider_source;
  }
  NOTREACHED();
  return content_settings::SETTING_SOURCE_NONE;
}

base::Value HostContentSettingsMap::GetWebsiteSettingInternal(
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    ProviderType first_provider_to_search,
    content_settings::SettingInfo* info) const {
  UsedContentSettingsProviders();
  ContentSettingsPattern* primary_pattern = nullptr;
  ContentSettingsPattern* secondary_pattern = nullptr;
  content_settings::RuleMetaData* metadata = nullptr;
  if (info) {
    primary_pattern = &info->primary_pattern;
    secondary_pattern = &info->secondary_pattern;
    metadata = &info->metadata;
  }

  // The list of |content_settings_providers_| is ordered according to their
  // precedence.
  auto it = content_settings_providers_.lower_bound(first_provider_to_search);
  for (; it != content_settings_providers_.end(); ++it) {
    base::Value value = GetContentSettingValueAndPatterns(
        it->second.get(), primary_url, secondary_url, content_type,
        is_off_the_record_, primary_pattern, secondary_pattern, metadata,
        clock_);
    if (!value.is_none()) {
      if (info)
        info->source = kProviderNamesSourceMap[it->first].provider_source;
      return value;
    }
  }

  if (info) {
    info->source = content_settings::SETTING_SOURCE_NONE;
    info->primary_pattern = ContentSettingsPattern();
    info->secondary_pattern = ContentSettingsPattern();
  }
  return base::Value();
}

// static
base::Value HostContentSettingsMap::GetContentSettingValueAndPatterns(
    const content_settings::ProviderInterface* provider,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsType content_type,
    bool include_incognito,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern,
    content_settings::RuleMetaData* metadata,
    base::Clock* clock) {
  // TODO(crbug.com/1336617): Remove this check once we figure out what is
  // wrong.
  CHECK(provider);

  if (include_incognito) {
    // Check incognito-only specific settings. It's essential that the
    // |RuleIterator| gets out of scope before we get a rule iterator for the
    // normal mode.
    std::unique_ptr<content_settings::RuleIterator> incognito_rule_iterator(
        provider->GetRuleIterator(content_type, true /* incognito */));
    base::Value value = GetContentSettingValueAndPatterns(
        incognito_rule_iterator.get(), primary_url, secondary_url,
        primary_pattern, secondary_pattern, metadata, clock);
    if (!value.is_none())
      return value;
  }
  // No settings from the incognito; use the normal mode.
  std::unique_ptr<content_settings::RuleIterator> rule_iterator(
      provider->GetRuleIterator(content_type, false /* incognito */));
  base::Value value = GetContentSettingValueAndPatterns(
      rule_iterator.get(), primary_url, secondary_url, primary_pattern,
      secondary_pattern, metadata, clock);
  if (!value.is_none() && include_incognito) {
    value = ProcessIncognitoInheritanceBehavior(content_type, std::move(value));
  }
  return value;
}

// static
base::Value HostContentSettingsMap::GetContentSettingValueAndPatterns(
    content_settings::RuleIterator* rule_iterator,
    const GURL& primary_url,
    const GURL& secondary_url,
    ContentSettingsPattern* primary_pattern,
    ContentSettingsPattern* secondary_pattern,
    content_settings::RuleMetaData* metadata,
    base::Clock* clock) {
  if (rule_iterator) {
    while (rule_iterator->HasNext()) {
      std::unique_ptr<content_settings::Rule> rule = rule_iterator->Next();
      // Refer to comment near the definition `kEagerExpiryBuffer` regarding
      // provisioning and CPU contention.
      if (rule->primary_pattern.Matches(primary_url) &&
          rule->secondary_pattern.Matches(secondary_url) &&
          (base::FeatureList::IsEnabled(
               content_settings::features::kActiveContentSettingExpiry) ||
           (rule->metadata.expiration().is_null() ||
            (rule->metadata.expiration() > clock->Now())))) {
        if (primary_pattern)
          *primary_pattern = rule->primary_pattern;
        if (secondary_pattern)
          *secondary_pattern = rule->secondary_pattern;
        if (metadata)
          *metadata = rule->metadata;
        return rule->TakeValue();
      }
    }
  }
  return base::Value();
}

void HostContentSettingsMap::
    MigrateSettingsPrecedingPermissionDelegationActivation() {
  auto* content_settings_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (const content_settings::ContentSettingsInfo* info :
       *content_settings_registry) {
    MigrateSingleSettingPrecedingPermissionDelegationActivation(
        info->website_settings_info());
  }

  auto* website_settings_registry =
      content_settings::WebsiteSettingsRegistry::GetInstance();
  for (const content_settings::WebsiteSettingsInfo* info :
       *website_settings_registry) {
    MigrateSingleSettingPrecedingPermissionDelegationActivation(info);
  }
}

void HostContentSettingsMap::
    MigrateSingleSettingPrecedingPermissionDelegationActivation(
        const content_settings::WebsiteSettingsInfo* info) {
  // Only migrate settings that don't support secondary patterns.
  if (info->SupportsSecondaryPattern())
    return;

  ContentSettingsType type = info->type();

  for (ContentSettingPatternSource pattern : GetSettingsForOneType(type)) {
    if (pattern.source != "preference" ||
        pattern.secondary_pattern == ContentSettingsPattern::Wildcard()) {
      continue;
    }

    if (pattern.secondary_pattern.IsValid() &&
        pattern.secondary_pattern != pattern.primary_pattern) {
      SetContentSettingCustomScope(pattern.primary_pattern,
                                   pattern.secondary_pattern, type,
                                   CONTENT_SETTING_DEFAULT);
      // Also clear the setting for the top level origin so that the user
      // receives another prompt. This is necessary in case they have allowed
      // the top level origin but blocked an embedded origin in which case
      // they should have another opportunity to block a request from an
      // embedded origin.
      SetContentSettingCustomScope(pattern.secondary_pattern,
                                   pattern.secondary_pattern, type,
                                   CONTENT_SETTING_DEFAULT);
      SetContentSettingCustomScope(pattern.secondary_pattern,
                                   ContentSettingsPattern::Wildcard(), type,
                                   CONTENT_SETTING_DEFAULT);
    } else if (pattern.primary_pattern.IsValid() &&
               pattern.primary_pattern == pattern.secondary_pattern) {
      // Migrate settings from (x,x) -> (x,*).
      SetContentSettingCustomScope(pattern.primary_pattern,
                                   pattern.secondary_pattern, type,
                                   CONTENT_SETTING_DEFAULT);
      SetContentSettingCustomScope(pattern.primary_pattern,
                                   ContentSettingsPattern::Wildcard(), type,
                                   pattern.GetContentSetting());
    }
  }
}

bool HostContentSettingsMap::IsSecondaryPatternAllowed(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const base::Value& value) {
  // A secondary pattern is normally only allowed if the content type supports
  // secondary patterns. One exception is made when deleting content settings
  // (aka setting them to CONTENT_SETTING_DEFAULT).
  return allow_invalid_secondary_pattern_for_testing_ ||
         secondary_pattern == ContentSettingsPattern::Wildcard() ||
         content_settings::WebsiteSettingsRegistry::GetInstance()
             ->Get(content_type)
             ->SupportsSecondaryPattern() ||
         content_settings::ValueToContentSetting(value) ==
             CONTENT_SETTING_DEFAULT;
}

void HostContentSettingsMap::UpdateExpiryEnforcementTimer(
    ContentSettingsType content_type,
    base::Time expiration) {
  if (expiration.is_null()) {
    return;
  }

  base::TimeDelta next_run = base::TimeDelta::Max();

  if (!base::Contains(expiration_enforcement_timers_, content_type)) {
    expiration_enforcement_timers_[content_type] =
        std::make_unique<base::OneShotTimer>();
  }

  auto& expiration_enforcement_timer =
      expiration_enforcement_timers_[content_type];

  if (expiration_enforcement_timer->IsRunning()) {
    next_run = expiration_enforcement_timer->GetCurrentDelay();
  }

  next_run =
      std::min(next_run, (expiration - clock_->Now() - kEagerExpiryBuffer));
  if (next_run.is_negative()) {
    next_run = base::Seconds(0);
  }

  expiration_enforcement_timer->Start(
      FROM_HERE, next_run,
      base::BindOnce(&HostContentSettingsMap::
                         DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun,
                     base::Unretained(this), content_type));
}

void HostContentSettingsMap::DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
    ContentSettingsType content_setting_type) {
  if (!base::FeatureList::IsEnabled(
          content_settings::features::kActiveContentSettingExpiry)) {
    return;
  }

  UsedContentSettingsProviders();
  base::Time next_expiry = base::Time::Max();

  std::vector<ContentSettingPatternSource> expired_entries;
  // Get content settings from all providers, so content setting expirations are
  // enforced at across all providers.
  auto settings = GetSettingsForOneType(content_setting_type);

  for (const ContentSettingPatternSource& setting : settings) {
    if (setting.metadata.expiration().is_null()) {
      continue;
    }

    if (setting.metadata.expiration() <= (clock_->Now() + kEagerExpiryBuffer)) {
      expired_entries.emplace_back(setting);
      content_settings_uma_util::RecordActiveExpiryEvent(
          GetProviderTypeFromSource(setting.source), content_setting_type);
    } else {
      next_expiry = std::min(next_expiry, setting.metadata.expiration());
    }
  }

  for (const auto& entry : expired_entries) {
    const auto provider_type = GetProviderTypeFromSource(entry.source);
    const bool is_user_modifiable =
        provider_type > kFirstUserModifiableProvider;

    if (is_user_modifiable) {
      static_cast<content_settings::UserModifiableProvider*>(
          content_settings_providers_[provider_type].get())
          ->ExpireWebsiteSetting(entry.primary_pattern, entry.secondary_pattern,
                                 content_setting_type);
    } else {
      // For non-modifiable providers there exists no expiry method and
      // SetWebsiteSettingCustomScope cannot work.
      NOTREACHED();
    }
  }

  if (!next_expiry.is_max()) {
    const base::TimeDelta next_run = std::max(
        base::Seconds(0), next_expiry - clock_->Now() - kEagerExpiryBuffer);

    if (!base::Contains(expiration_enforcement_timers_, content_setting_type)) {
      expiration_enforcement_timers_[content_setting_type] =
          std::make_unique<base::OneShotTimer>();
    }

    auto& expiration_enforcement_timer =
        expiration_enforcement_timers_[content_setting_type];
    expiration_enforcement_timer->Start(
        FROM_HERE, next_run,
        base::BindOnce(&HostContentSettingsMap::
                           DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun,
                       base::Unretained(this), content_setting_type));
  }
}
