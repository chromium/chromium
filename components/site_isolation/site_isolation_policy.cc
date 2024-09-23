// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/site_isolation/site_isolation_policy.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/site_isolation/features.h"
#include "components/site_isolation/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"

namespace site_isolation {

namespace {

using IsolatedOriginSource =
    content::ChildProcessSecurityPolicy::IsolatedOriginSource;

bool g_disallow_memory_threshold_caching_for_testing = false;

struct IsolationDisableDecisions {
  bool should_disable_strict;
  bool should_disable_partial;
};

bool ShouldDisableSiteIsolationDueToMemorySlow(
    content::SiteIsolationMode site_isolation_mode) {
  // The memory threshold behavior differs for desktop and Android:
  // - Android uses a 1900MB default threshold for partial site isolation modes
  //   and a 3200MB default threshold for strict site isolation. See docs in
  //   https://crbug.com/849815. The thresholds roughly correspond to 2GB+ and
  //   4GB+ devices and are lower to account for memory carveouts, which
  //   reduce the amount of memory seen by AmountOfPhysicalMemoryMB(). Both
  //   partial and strict site isolation thresholds can be overridden via
  //   params defined in a kSiteIsolationMemoryThresholds field trial.
  // - Desktop does not enforce a default memory threshold, but for now we
  //   still support a threshold defined via a kSiteIsolationMemoryThresholds
  //   field trial.  The trial typically carries the threshold in a param; if
  //   it doesn't, use a default that's slightly higher than 1GB (see
  //   https://crbug.com/844118).
  int default_memory_threshold_mb;
#if BUILDFLAG(IS_ANDROID)
  if (site_isolation_mode == content::SiteIsolationMode::kStrictSiteIsolation) {
    default_memory_threshold_mb = 3200;
  } else {
    default_memory_threshold_mb = 1900;
  }
#else
  default_memory_threshold_mb = 1077;
#endif

  if (base::FeatureList::IsEnabled(features::kSiteIsolationMemoryThresholds)) {
    std::string param_name;
    switch (site_isolation_mode) {
      case content::SiteIsolationMode::kStrictSiteIsolation:
        param_name = features::kStrictSiteIsolationMemoryThresholdParamName;
        break;
      case content::SiteIsolationMode::kPartialSiteIsolation:
        param_name = features::kPartialSiteIsolationMemoryThresholdParamName;
        break;
    }
    int memory_threshold_mb = base::GetFieldTrialParamByFeatureAsInt(
        features::kSiteIsolationMemoryThresholds, param_name,
        default_memory_threshold_mb);
    return base::SysInfo::AmountOfPhysicalMemoryMB() <= memory_threshold_mb;
  }

#if BUILDFLAG(IS_ANDROID)
  if (base::SysInfo::AmountOfPhysicalMemoryMB() <=
      default_memory_threshold_mb) {
    return true;
  }
#endif

  return false;
}

IsolationDisableDecisions MakeBothDecisions() {
  IsolationDisableDecisions result{
      .should_disable_strict = ShouldDisableSiteIsolationDueToMemorySlow(
          content::SiteIsolationMode::kStrictSiteIsolation),
      .should_disable_partial = ShouldDisableSiteIsolationDueToMemorySlow(
          content::SiteIsolationMode::kPartialSiteIsolation)};
  return result;
}

bool CachedDisableSiteIsolation(
    content::SiteIsolationMode site_isolation_mode) {
  static const IsolationDisableDecisions decisions = MakeBothDecisions();
  if (site_isolation_mode == content::SiteIsolationMode::kStrictSiteIsolation) {
    return decisions.should_disable_strict;
  }
  return decisions.should_disable_partial;
}

}  // namespace

// static
bool SiteIsolationPolicy::IsIsolationForPasswordSitesEnabled() {
  // If the user has explicitly enabled site isolation for password sites from
  // chrome://flags or from the command line, honor this regardless of policies
  // that may disable site isolation.  In particular, this means that the
  // chrome://flags switch for this feature takes precedence over any memory
  // threshold restrictions and over a switch for disabling site isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kSiteIsolationForPasswordSites.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // Don't isolate anything when site isolation is turned off by the user or
  // policy. This includes things like the switches::kDisableSiteIsolation
  // command-line switch, the corresponding "Disable site isolation" entry in
  // chrome://flags, enterprise policy controlled via
  // switches::kDisableSiteIsolationForPolicy, and memory threshold checks in
  // ShouldDisableSiteIsolationDueToMemoryThreshold().
  if (!content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kSiteIsolationForPasswordSites);
}

// static
bool SiteIsolationPolicy::IsIsolationForOAuthSitesEnabled() {
  // If the user has explicitly enabled site isolation for OAuth sites from the
  // command line, honor this regardless of policies that may disable site
  // isolation.
  if (base::FeatureList::GetInstance()->IsFeatureOverriddenFromCommandLine(
          features::kSiteIsolationForOAuthSites.name,
          base::FeatureList::OVERRIDE_ENABLE_FEATURE)) {
    return true;
  }

  // Don't isolate anything when site isolation is turned off by the user or
  // policy. This includes things like the switches::kDisableSiteIsolation
  // command-line switch, the corresponding "Disable site isolation" entry in
  // chrome://flags, enterprise policy controlled via
  // switches::kDisableSiteIsolationForPolicy, and memory threshold checks in
  // ShouldDisableSiteIsolationDueToMemoryThreshold().
  if (!content::SiteIsolationPolicy::AreDynamicIsolatedOriginsEnabled())
    return false;

  // The feature needs to be checked last, because checking the feature
  // activates the field trial and assigns the client either to a control or an
  // experiment group - such assignment should be final.
  return base::FeatureList::IsEnabled(features::kSiteIsolationForOAuthSites);
}

// static
bool SiteIsolationPolicy::IsEnterprisePolicyApplicable() {
#if BUILDFLAG(IS_ANDROID)
  // https://crbug.com/844118: Limiting policy to devices with > 1GB RAM.
  // Using 1077 rather than 1024 because it helps ensure that devices with
  // exactly 1GB of RAM won't get included because of inaccuracies or off-by-one
  // errors.
  bool have_enough_memory = base::SysInfo::AmountOfPhysicalMemoryMB() > 1077;
  return have_enough_memory;
#else
  return true;
#endif
}

// static
bool SiteIsolationPolicy::ShouldDisableSiteIsolationDueToMemoryThreshold(
    content::SiteIsolationMode site_isolation_mode) {
  if (!g_disallow_memory_threshold_caching_for_testing) {
    return CachedDisableSiteIsolation(site_isolation_mode);
  }
  return ShouldDisableSiteIsolationDueToMemorySlow(site_isolation_mode);
}

// static
void SiteIsolationPolicy::PersistIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin,
    IsolatedOriginSource source) {
  DCHECK(context);
  DCHECK(!context->IsOffTheRecord());
  DCHECK(!origin.opaque());

  // This function currently supports two sources for persistence, for
  // user-triggered and web-triggered isolated origins.
  if (source == IsolatedOriginSource::USER_TRIGGERED) {
    PersistUserTriggeredIsolatedOrigin(context, origin);
  } else if (source == IsolatedOriginSource::WEB_TRIGGERED) {
    PersistWebTriggeredIsolatedOrigin(context, origin);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

// static
void SiteIsolationPolicy::PersistUserTriggeredIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin) {
  // User-triggered isolated origins are currently stored in a simple list of
  // unlimited size.
  // TODO(alexmos): Cap the maximum number of entries and evict older entries.
  // See https://crbug.com/1172407.
  ScopedListPrefUpdate update(
      user_prefs::UserPrefs::Get(context),
      site_isolation::prefs::kUserTriggeredIsolatedOrigins);
  base::Value::List& list = update.Get();
  base::Value value(origin.Serialize());
  if (!base::Contains(list, value))
    list.Append(std::move(value));
}

// static
void SiteIsolationPolicy::PersistWebTriggeredIsolatedOrigin(
    content::BrowserContext* context,
    const url::Origin& origin) {
  // Web-triggered isolated origins are stored in a dictionary of (origin,
  // timestamp) pairs.  The number of entries is capped by a field trial param,
  // and older entries are evicted.
  ScopedDictPrefUpdate update(
      user_prefs::UserPrefs::Get(context),
      site_isolation::prefs::kWebTriggeredIsolatedOrigins);
  base::Value::Dict& dict = update.Get();

  // Add the origin.  If it already exists, this will just update the
  // timestamp.
  dict.Set(origin.Serialize(), base::TimeToValue(base::Time::Now()));

  // Check whether the maximum number of stored sites was exceeded and remove
  // one or more entries, starting with the oldest timestamp. Note that more
  // than one entry may need to be removed, since the maximum number of entries
  // could change over time (via a change in the field trial param).
  size_t max_size =
      ::features::kSiteIsolationForCrossOriginOpenerPolicyMaxSitesParam.Get();
  while (dict.size() > max_size) {
    auto oldest_site_time_pair = std::min_element(
        dict.begin(), dict.end(), [](auto pair_a, auto pair_b) {
          std::optional<base::Time> time_a = base::ValueToTime(pair_a.second);
          std::optional<base::Time> time_b = base::ValueToTime(pair_b.second);
          // has_value() should always be true unless the prefs were corrupted.
          // In that case, prioritize the corrupted entry for removal.
          return (time_a.has_value() ? time_a.value() : base::Time::Min()) <
                 (time_b.has_value() ? time_b.value() : base::Time::Min());
        });
    dict.erase(oldest_site_time_pair);
  }
}

// static
void SiteIsolationPolicy::ApplyPersistedIsolatedOrigins(
    content::BrowserContext* browser_context) {
  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();

  // If the user turned off password-triggered isolation, don't apply any
  // stored isolated origins, but also don't clear them from prefs, so that
  // they can be used if password-triggered isolation is re-enabled later.
  if (IsIsolationForPasswordSitesEnabled()) {
    std::vector<url::Origin> origins;
    for (const auto& value :
         user_prefs::UserPrefs::Get(browser_context)
             ->GetList(prefs::kUserTriggeredIsolatedOrigins)) {
      origins.push_back(url::Origin::Create(GURL(value.GetString())));
    }

    if (!origins.empty()) {
      policy->AddFutureIsolatedOrigins(
          origins, IsolatedOriginSource::USER_TRIGGERED, browser_context);
    }

    base::UmaHistogramCounts1000(
        "SiteIsolation.SavedUserTriggeredIsolatedOrigins.Size", origins.size());
  }

  // Similarly, load saved web-triggered isolated origins only if isolation of
  // COOP sites (currently the only source of these origins) is enabled with
  // persistence, but don't remove them from prefs otherwise.
  if (content::SiteIsolationPolicy::ShouldPersistIsolatedCOOPSites()) {
    std::vector<url::Origin> origins;
    std::vector<std::string> expired_entries;

    auto* pref_service = user_prefs::UserPrefs::Get(browser_context);
    const auto& dict =
        pref_service->GetDict(prefs::kWebTriggeredIsolatedOrigins);
    for (auto site_time_pair : dict) {
      // Only isolate origins that haven't expired.
      std::optional<base::Time> timestamp =
          base::ValueToTime(site_time_pair.second);
      base::TimeDelta expiration_timeout =
          ::features::
              kSiteIsolationForCrossOriginOpenerPolicyExpirationTimeoutParam
                  .Get();
      if (timestamp.has_value() &&
          base::Time::Now() - timestamp.value() <= expiration_timeout) {
        origins.push_back(url::Origin::Create(GURL(site_time_pair.first)));
      } else {
        expired_entries.push_back(site_time_pair.first);
      }
    }
    // Remove expired entries (as well as those with an invalid timestamp).
    if (!expired_entries.empty()) {
      ScopedDictPrefUpdate update(pref_service,
                                  prefs::kWebTriggeredIsolatedOrigins);
      base::Value::Dict& updated_dict = update.Get();
      for (const auto& entry : expired_entries)
        updated_dict.Remove(entry);
    }

    if (!origins.empty()) {
      policy->AddFutureIsolatedOrigins(
          origins, IsolatedOriginSource::WEB_TRIGGERED, browser_context);
    }

    base::UmaHistogramCounts100(
        "SiteIsolation.SavedWebTriggeredIsolatedOrigins.Size", origins.size());
  }
}

// static
void SiteIsolationPolicy::IsolateStoredOAuthSites(
    content::BrowserContext* browser_context,
    const std::vector<url::Origin>& logged_in_sites) {
  // Only isolate logged-in sites if the corresponding feature is enabled and
  // other isolation requirements (such as memory threshold) are satisfied.
  // Note that we don't clear logged-in sites from prefs if site isolation is
  // disabled so that they can be used if isolation is re-enabled later.
  if (!IsIsolationForOAuthSitesEnabled())
    return;

  auto* policy = content::ChildProcessSecurityPolicy::GetInstance();
  policy->AddFutureIsolatedOrigins(
      logged_in_sites,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::USER_TRIGGERED,
      browser_context);

  // Note that the max count matches
  // login_detection::GetOauthLoggedInSitesMaxSize().
  base::UmaHistogramCounts100("SiteIsolation.SavedOAuthSites.Size",
                              logged_in_sites.size());
}

// static
void SiteIsolationPolicy::IsolateNewOAuthURL(
    content::BrowserContext* browser_context,
    const GURL& signed_in_url) {
  if (!IsIsolationForOAuthSitesEnabled())
    return;

  // OAuth information is currently persisted and restored by other layers. See
  // login_detection::prefs::SaveSiteToOAuthSignedInList().
  constexpr bool kShouldPersist = false;

  content::SiteInstance::StartIsolatingSite(
      browser_context, signed_in_url,
      content::ChildProcessSecurityPolicy::IsolatedOriginSource::USER_TRIGGERED,
      kShouldPersist);
}

// static
bool SiteIsolationPolicy::ShouldPdfCompositorBeEnabledForOopifs() {
#if BUILDFLAG(IS_ANDROID)
  // TODO(crbug.com/40657857): Always enable on Android, at which point, this
  // method should go away.
  //
  // Only use the PDF compositor when one of the site isolation modes that
  // forces OOPIFs is on. This includes:
  // - Full site isolation, which may be forced on.
  // - Password-triggered site isolation for high-memory devices
  // - Isolated origins specified via command line, enterprise policy, or field
  //   trials.
  return content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites() ||
         IsIsolationForPasswordSitesEnabled() ||
         content::SiteIsolationPolicy::AreIsolatedOriginsEnabled();
#else
  // Always use the PDF compositor on non-mobile platforms.
  return true;
#endif
}

// static
void SiteIsolationPolicy::SetDisallowMemoryThresholdCachingForTesting(
    bool disallow_caching) {
  g_disallow_memory_threshold_caching_for_testing = disallow_caching;
}

}  // namespace site_isolation
