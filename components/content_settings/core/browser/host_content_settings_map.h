// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Maps hostnames to custom content settings.  Written on the UI thread and read
// on any thread.  One instance per profile.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/check_is_test.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/user_modifiable_provider.h"

#include "components/content_settings/core/common/content_settings_constraints.h"
#include "components/content_settings/core/common/content_settings_metadata.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

// In the context of active expiry enforcement, content settings are considered
// expired if their expiration time is before 'Now() + `kEagerExpiryBuffer` at
// the time of check. This value accounts for posted task delays due to
// prioritization. Note that there are no guarantees about CPU contention, so
// this doesn't prevent occasional outlier tasks that are delayed further.
// However, consistency is more important then accuracy in this context.
// Expirations are not user visible, and are not guarantees given to or chosen
// by the user. If we do not delete but also don't provide, we get into an
// inconsistent state which among other issues is also a security concern: the
// expired content setting is no longer provided and thus isn't listed in page
// info even though the site might still have active access to it. At this
// point, the only way the user can block access to the permission is to
// reload, navigate away or close the tab/browser. These are not reasonable user
// journeys. Additionally, if CPU contention leads to significant delays in the
// run loop, other browser process tasks such as checking content settings
// are very likely similarly delayed.
static constexpr base::TimeDelta kEagerExpiryBuffer = base::Seconds(5);

class GURL;
class PrefService;

namespace base {
class Value;
class Clock;
}  // namespace base

namespace content_settings {
class ObservableProvider;
class ProviderInterface;
class PrefProvider;
class TestUtils;
class WebsiteSettingsInfo;
}  // namespace content_settings

namespace user_prefs {
class PrefRegistrySyncable;
}

class HostContentSettingsMap : public content_settings::Observer,
                               public RefcountedKeyedService {
 public:
  using ProviderType = content_settings::ProviderType;

  // This should be called on the UI thread, otherwise |thread_checker_| handles
  // CalledOnValidThread() wrongly. |is_off_the_record| indicates incognito
  // profile or a guest session.
  HostContentSettingsMap(PrefService* prefs,
                         bool is_off_the_record,
                         bool store_last_modified,
                         bool restore_session,
                         bool should_record_metrics);

  HostContentSettingsMap(const HostContentSettingsMap&) = delete;
  HostContentSettingsMap& operator=(const HostContentSettingsMap&) = delete;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Adds a new provider for |type|. This should be used instead of
  // |RegisterProvider|, not in addition.
  //
  // Providers added via this method may be cleared by
  // |ClearSettingsForOneTypeWithPredicate| if they were recently modified.
  void RegisterUserModifiableProvider(
      ProviderType type,
      std::unique_ptr<content_settings::UserModifiableProvider> provider);

  // Adds a new provider for |type|.
  void RegisterProvider(
      ProviderType type,
      std::unique_ptr<content_settings::ObservableProvider> provider);

  // Returns the default setting for a particular content type. If
  // |provider_type| is not NULL, the id of the provider which provided the
  // default setting is assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultContentSetting(
      ContentSettingsType content_type,
      ProviderType* provider_id = nullptr) const;

  // Returns a single |ContentSetting| which applies to the given URLs.  Note
  // that certain internal schemes are allowlisted. For |CONTENT_TYPE_COOKIES|,
  // |CookieSettings| should be used instead. For content types that can't be
  // converted to a |ContentSetting|, |GetContentSettingValue| should be called.
  // If there is no content setting, returns CONTENT_SETTING_DEFAULT. |info| is
  // populated as explained in |GetWebsiteSetting()|.
  //
  // May be called on any thread.
  ContentSetting GetContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info = nullptr) const;

  // This is the same as GetContentSetting() but ignores providers which are not
  // user-controllable (e.g. policy and extensions).
  ContentSetting GetUserModifiableContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type) const;

  // Returns a single content setting |Value| which applies to the given URLs.
  // If |info| is not NULL, then the |source| field of |info| is set to the
  // source of the returned |Value| (POLICY, EXTENSION, USER, ...) and the
  // |primary_pattern| and the |secondary_pattern| fields of |info| are set to
  // the patterns of the applying rule.  Note that certain internal schemes are
  // allowlisted. For allowlisted schemes the |source| field of |info| is set
  // the |SettingSource::kAllowList| and the |primary_pattern| and
  // |secondary_pattern| are set to a wildcard pattern.  If there is no content
  // setting, a NONE-type value is returned and the |source| field of |info| is
  // set to |SettingSource::kNone|. The pattern fields of |info| are set to
  // empty patterns. May be called on any thread.
  base::Value GetWebsiteSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      content_settings::SettingInfo* info = nullptr) const;

  // For a given content type, returns all patterns with a non-default setting,
  // mapped to their actual settings, in the precedence order of the rules.
  // |session_model| can be specified to limit the type of setting results
  // returned. Any entries in the returned value are guaranteed to be unexpired
  // at the time they are retrieved from their respective providers and
  // incognito inheritance behavior is applied. If the returned settings are not
  // used immediately the validity of each entry should be checked using
  // IsExpired().
  //
  // The intended purpose of this method is to display a list of settings in the
  // settings UI. It should not be used to evaluate whether settings are
  // enabled. Use GetWebsiteSetting for that.
  //
  // This may be called on any thread.
  ContentSettingsForOneType GetSettingsForOneType(
      ContentSettingsType content_type,
      std::optional<content_settings::mojom::SessionModel> session_model =
          std::nullopt) const;

  // Returns the correct patterns for the scoping of the particular content
  // type.
  static content_settings::PatternPair GetPatternsForContentSettingsType(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType type);

  // Sets the default setting for a particular content type. This method must
  // not be invoked on an incognito map.
  //
  // This should only be called on the UI thread.
  void SetDefaultContentSetting(ContentSettingsType content_type,
                                ContentSetting setting);

  // Sets the content |setting| for the given patterns and|content_type|
  // applying any provided |constraints|. Setting the value to
  // CONTENT_SETTING_DEFAULT causes the default setting for that type to be used
  // when loading pages matching this pattern. Unless adding a custom-scoped
  // setting, most developers will want to use SetContentSettingDefaultScope()
  // instead.
  //
  // NOTICE: This is just a convenience method for content types that use
  // |ContentSetting| as their data type. For content types that use other data
  // types (e.g. arbitrary base::Values) please use the method
  // SetWebsiteSettingDefaultScope().
  //
  // This should only be called on the UI thread.
  void SetContentSettingCustomScope(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      ContentSetting setting,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Sets the content |setting| for the default scope of the url that is
  // appropriate for the given |content_type| applying any provided
  // |constraints|. Setting the value to CONTENT_SETTING_DEFAULT causes the
  // default setting for that type to be used.
  //
  // NOTICE: This is just a convenience method for content types that use
  // |ContentSetting| as their data type. For content types that use other data
  // types (e.g. arbitrary base::Values) please use the method
  // SetWebsiteSettingDefaultScope().
  //
  // This should only be called on the UI thread.
  //
  // Internally this will call SetContentSettingCustomScope() with the default
  // scope patterns for the given |content_type|. Developers will generally want
  // to use this function instead of SetContentSettingCustomScope() unless they
  // need to specify custom scoping.
  void SetContentSettingDefaultScope(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      ContentSetting setting,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Sets the |value| for the default scope of the url that is appropriate for
  // the given |content_type| applying any provided |constraints|. Setting the
  // value to NONE (base::Value()) removes the default pattern pair for this
  // content type.
  //
  // Internally this will call SetWebsiteSettingCustomScope() with the default
  // scope patterns for the given |content_type|. Developers will generally want
  // to use this function instead of SetWebsiteSettingCustomScope() unless they
  // need to specify custom scoping.
  void SetWebsiteSettingDefaultScope(
      const GURL& requesting_url,
      const GURL& top_level_url,
      ContentSettingsType content_type,
      base::Value value,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Sets a rule to apply the |value| for all sites matching |pattern|,
  // |content_type| applying any provided |constraints|. Setting the value to
  // NONE removes the given pattern pair. Unless adding a custom-scoped setting,
  // most developers will want to use SetWebsiteSettingDefaultScope() instead.
  void SetWebsiteSettingCustomScope(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      base::Value value,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Check if a call to SetNarrowestContentSetting would succeed or if it would
  // fail because of an invalid pattern.
  bool CanSetNarrowestContentSetting(const GURL& primary_url,
                                     const GURL& secondary_url,
                                     ContentSettingsType type) const;

  // Checks whether the specified |type| controls a feature that is restricted
  // to secure origins.
  bool IsRestrictedToSecureOrigins(ContentSettingsType type) const;

  // Sets the most specific rule that currently defines the setting for the
  // given content type. TODO(raymes): Remove this once all content settings
  // are scoped to origin scope. There is no scope more narrow than origin
  // scope, so we can just blindly set the value of the origin scope when that
  // happens.
  void SetNarrowestContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType type,
      ContentSetting setting,
      const content_settings::ContentSettingConstraints& constraints = {});

  // Updates the last used time to a recent timestamp.
  void UpdateLastUsedTime(const GURL& primary_url,
                          const GURL& secondary_url,
                          ContentSettingsType type,
                          const base::Time time);

  // Reset the last visited time to base::Time().
  void ResetLastVisitedTime(const ContentSettingsPattern& primary_pattern,
                            const ContentSettingsPattern& secondary_pattern,
                            ContentSettingsType type);
  // Updates the last visited time to a recent coarse timestamp
  // (week-precision).
  void UpdateLastVisitedTime(const ContentSettingsPattern& primary_pattern,
                             const ContentSettingsPattern& secondary_pattern,
                             ContentSettingsType type);

  // Updates the expiration to `lifetime + now()`, if `setting_to_match` is
  // nullopt or if it matches the rule's value. Returns the TimeDelta between
  // now and the setting's old expiration time if any setting was matched and
  // updated; nullopt otherwise.
  std::optional<base::TimeDelta> RenewContentSetting(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType type,
      std::optional<ContentSetting> setting_to_match);

  // Clears all host-specific settings for one content type.
  //
  // This should only be called on the UI thread.
  void ClearSettingsForOneType(ContentSettingsType content_type);

  using PatternSourcePredicate = base::RepeatingCallback<bool(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern)>;

  // If |pattern_predicate| is null, this method is equivalent to the above.
  // Otherwise, it only deletes exceptions matched by |pattern_predicate| that
  // were modified at or after |begin_time| and before |end_time|. To delete
  // an individual setting, use SetWebsiteSetting/SetContentSetting methods.
  void ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType content_type,
      base::Time begin_time,
      base::Time end_time,
      PatternSourcePredicate pattern_predicate);

  // Clears all host-specific settings for one content type which also satisfy a
  // predicate.
  void ClearSettingsForOneTypeWithPredicate(
      ContentSettingsType content_type,
      base::FunctionRef<bool(const ContentSettingPatternSource&)> predicate);

  // RefcountedKeyedService implementation.
  void ShutdownOnUIThread() override;

  // content_settings::Observer implementation.
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override;


  // Whether this settings map is for an incognito or guest session.
  bool IsOffTheRecord() const { return is_off_the_record_; }

  // Adds/removes an observer for content settings changes.
  void AddObserver(content_settings::Observer* observer);
  void RemoveObserver(content_settings::Observer* observer);

  // Schedules any pending lossy website settings to be written to disk.
  void FlushLossyWebsiteSettings();

  base::WeakPtr<HostContentSettingsMap> GetWeakPtr();

  // Injects a clock into the PrefProvider to allow control over the
  // |last_modified| timestamp.
  void SetClockForTesting(const base::Clock* clock);

  // Returns the provider that contains content settings from user
  // preferences.
  content_settings::PrefProvider* GetPrefProvider() const {
    return pref_provider_;
  }

  // Only use for testing.
  void AllowInvalidSecondaryPatternForTesting(bool allow) {
    allow_invalid_secondary_pattern_for_testing_ = allow;
  }

  // Returns the current time of the `clock_`.
  base::Time Now() const { return clock_->Now(); }

 private:
  friend class base::RefCountedThreadSafe<HostContentSettingsMap>;
  friend class content_settings::TestUtils;
  friend class HostContentSettingsMapActiveExpirationTest;
  friend class OneTimePermissionExpiryEnforcementUmaInteractiveUiTest;

  FRIEND_TEST_ALL_PREFIXES(
      OneTimePermissionExpiryEnforcementUmaInteractiveUiTest,
      TestExpiryEnforcement);
  FRIEND_TEST_ALL_PREFIXES(HostContentSettingsMapTest,
                           MigrateRequestingAndTopLevelOriginSettings);
  FRIEND_TEST_ALL_PREFIXES(
      HostContentSettingsMapTest,
      MigrateRequestingAndTopLevelOriginSettingsResetsEmbeddedSetting);

  ~HostContentSettingsMap() override;

  ContentSetting GetDefaultContentSettingFromProvider(
      ContentSettingsType content_type,
      content_settings::ProviderInterface* provider) const;

  // Retrieves default content setting for |content_type|, and writes the
  // provider's type to |provider_type| (must not be null).
  ContentSetting GetDefaultContentSettingInternal(
      ContentSettingsType content_type,
      ProviderType* provider_type) const;

  // Collect UMA data of exceptions.
  void RecordExceptionMetrics();
  // Collect UMA data for 3PC exceptions.
  void RecordThirdPartyCookieMetrics(const ContentSettingsForOneType& settings);

  // Adds content settings for |content_type| provided by |provider|, into
  // |settings|. If |incognito| is true, adds only the content settings which
  // are applicable to the incognito mode and differ from the normal mode.
  // Otherwise, adds the content settings for the normal mode (applying
  // inheritance rules if |is_off_the_record_|).
  void AddSettingsForOneType(
      const content_settings::ProviderInterface* provider,
      ProviderType provider_type,
      ContentSettingsType content_type,
      ContentSettingsForOneType* settings,
      bool incognito,
      std::optional<content_settings::mojom::SessionModel> session_model) const;

  // Call UsedContentSettingsProviders() whenever you access
  // content_settings_providers_ (apart from initialization and
  // teardown), so that we can DCHECK in RegisterExtensionService that
  // it is not being called too late.
  void UsedContentSettingsProviders() const;

  // Returns the single content setting |value| with a toggle for if it
  // takes the global on/off switch into account.
  base::Value GetWebsiteSettingInternal(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      ProviderType first_provider_to_search,
      content_settings::SettingInfo* info) const;

  content_settings::PatternPair GetNarrowestPatterns(
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType type) const;

  static base::Value GetContentSettingValueAndPatterns(
      const content_settings::ProviderInterface* provider,
      const GURL& primary_url,
      const GURL& secondary_url,
      ContentSettingsType content_type,
      bool include_incognito,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern,
      content_settings::RuleMetaData* metadata);

  static base::Value GetContentSettingValueAndPatterns(
      content_settings::Rule* rule,
      ContentSettingsPattern* primary_pattern,
      ContentSettingsPattern* secondary_pattern,
      content_settings::RuleMetaData* metadata);

  // Migrate requesting and top level origin content settings to remove all
  // settings that have a top level pattern. If there is a pattern set for
  // (http://x.com, http://y.com) this will remove that pattern and also remove
  // (http://y.com, *). The reason the second pattern is removed is to ensure
  // that permission won't automatically be granted to x.com when it's embedded
  // in y.com when permission delegation is enabled.
  // It also ensures that we move away from (http://x.com, http://x.com)
  // patterns by replacing these patterns with (http://x.com, *).
  void MigrateSettingsPrecedingPermissionDelegationActivation();
  void MigrateSingleSettingPrecedingPermissionDelegationActivation(
      const content_settings::WebsiteSettingsInfo* info);

  // Verifies that this secondary pattern is allowed.
  bool IsSecondaryPatternAllowed(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsType content_type,
      const base::Value& value);

  // For the content setting `content_type` with expiry `expiration`, sets or
  // updates the next run of expiration enforcement if required.
  void UpdateExpiryEnforcementTimer(ContentSettingsType content_type,
                                    base::Time expiration);

  // If the feature
  // `kActiveContentSettingExpiry` is enabled,
  // this method checks for and deletes all
  // content setting entries which will have
  // expired before `now() +
  // kEagerExpiryBuffer` in any provider. It
  // also determines the time of the next
  // future expiry and schedules itself to run
  // at `expiration() - kEagerExpiryBuffer` if
  // such a closest expiry exists for other
  // content setting entries of this type in
  // any provider. This method can and should
  // be called each time a new expiration
  // metadata field may be set for the
  // provider. It aborts and potentially
  // reinitializes running OneShotTimers
  // automatically in those cases.
  void DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun(
      ContentSettingsType content_setting_type);

#ifndef NDEBUG
  // This starts as the thread ID of the thread that constructs this
  // object, and remains until used by a different thread, at which
  // point it is set to base::kInvalidThreadId. This allows us to
  // DCHECK on unsafe usage of content_settings_providers_ (they
  // should be set up on a single thread, after which they are
  // immutable).
  mutable base::PlatformThreadId used_from_thread_id_;
#endif

  // Weak; owned by the Profile.
  raw_ptr<PrefService> prefs_;

  // Whether this settings map is for an incognito or guest session.
  bool is_off_the_record_;

  // Whether ContentSettings in the PrefProvider will store a last_modified
  // timestamp.
  bool store_last_modified_;

  // Content setting providers. This is only modified at construction
  // time and by RegisterExtensionService, both of which should happen
  // before any other uses of it.
  std::map<ProviderType, std::unique_ptr<content_settings::ProviderInterface>>
      content_settings_providers_;

  // List of content settings providers containing settings which can be
  // modified by the user. Members are owned by the
  // |content_settings_providers_| map above.
  std::vector<
      raw_ptr<content_settings::UserModifiableProvider, VectorExperimental>>
      user_modifiable_providers_;

  // content_settings_providers_[PREF_PROVIDER] but specialized.
  raw_ptr<content_settings::PrefProvider, DanglingUntriaged> pref_provider_ =
      nullptr;

  base::ThreadChecker thread_checker_;

  base::ObserverList<content_settings::Observer>::UncheckedAndDanglingUntriaged
      observers_;

  // When true, allows setting secondary patterns even for types that should not
  // allow them. Only used for testing that inserts previously valid patterns in
  // order to ensure the migration logic is sound.
  bool allow_invalid_secondary_pattern_for_testing_;

  raw_ptr<const base::Clock> clock_;

  // Maps content setting type to OneShotTimers that are used to run
  // `DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun` which checks for, and
  // deletes expired entries of the content setting if the feature flag
  // `kActiveContentSettingExpiry` is enabled.
  std::map<ContentSettingsType, std::unique_ptr<base::OneShotTimer>>
      expiration_enforcement_timers_;

  base::WeakPtrFactory<HostContentSettingsMap> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_HOST_CONTENT_SETTINGS_MAP_H_
