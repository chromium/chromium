// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/cookie_settings_base.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace content_settings {

// This enum is used in prefs, do not change values.
// The enum needs to correspond to CookieControlsMode in enums.xml.
enum class CookieControlsMode {
  kOff = 0,
  kOn = 1,
  kIncognitoOnly = 2,
  kMaxValue = kIncognitoOnly,
};

// Default value for |extension_scheme|.
const char kDummyExtensionScheme[] = ":no-extension-scheme:";

// A frontend to the cookie settings of |HostContentSettingsMap|. Handles
// cookie-specific logic such as blocking third-party cookies. Written on the UI
// thread and read on any thread.
class CookieSettings : public CookieSettingsBase,
                       public RefcountedKeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnThirdPartyCookieBlockingChanged(
        bool block_third_party_cookies) = 0;
  };

  // Creates a new CookieSettings instance.
  // The caller is responsible for ensuring that |extension_scheme| is valid for
  // the whole lifetime of this instance.
  // |is_incognito| indicates whether this is an incognito profile. It is not
  // true for other types of off-the-record profiles like guest mode.
  CookieSettings(HostContentSettingsMap* host_content_settings_map,
                 PrefService* prefs,
                 bool is_incognito,
                 const char* extension_scheme = kDummyExtensionScheme);

  // Returns the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies. If
  // |provider_id| is not nullptr, the id of the provider which provided the
  // default setting is assigned to it.
  //
  // This may be called on any thread.
  ContentSetting GetDefaultCookieSetting(std::string* provider_id) const;

  // Returns all patterns with a non-default cookie setting, mapped to their
  // actual settings, in the precedence order of the setting rules. |settings|
  // must be a non-nullptr outparam.
  //
  // This may be called on any thread.
  void GetCookieSettings(ContentSettingsForOneType* settings) const;

  // Sets the default content setting (CONTENT_SETTING_ALLOW,
  // CONTENT_SETTING_BLOCK, or CONTENT_SETTING_SESSION_ONLY) for cookies.
  //
  // This should only be called on the UI thread.
  void SetDefaultCookieSetting(ContentSetting setting);

  // Sets the cookie setting for the given url.
  //
  // This should only be called on the UI thread.
  void SetCookieSetting(const GURL& primary_url, ContentSetting setting);

  // Resets the cookie setting for the given url.
  //
  // This should only be called on the UI thread.
  void ResetCookieSetting(const GURL& primary_url);

  // Returns true if cookies are allowed for *most* third parties on |url|.
  // There might be rules allowing or blocking specific third parties from
  // accessing cookies.
  //
  // This should only be called on the UI thread.
  bool IsThirdPartyAccessAllowed(const GURL& first_party_url);

  // Sets the cookie setting for the site and third parties embedded in it.
  //
  // This should only be called on the UI thread.
  void SetThirdPartyCookieSetting(const GURL& first_party_url,
                                  ContentSetting setting);

  // Resets the third party cookie setting for the given url.
  //
  // This should only be called on the UI thread.
  void ResetThirdPartyCookieSetting(const GURL& first_party_url);

  bool IsStorageDurable(const GURL& origin) const;

  // Returns true if third party cookies should be blocked.
  //
  // This method may be called on any thread.
  bool ShouldBlockThirdPartyCookies() const;

  // content_settings::CookieSettingsBase:
  void GetSettingForLegacyCookieAccess(const std::string& cookie_domain,
                                       ContentSetting* setting) const override;
  bool ShouldIgnoreSameSiteRestrictions(
      const GURL& url,
      const GURL& site_for_cookies) const override;

  // Detaches the |CookieSettings| from |PrefService|. This methods needs to be
  // called before destroying the service. Afterwards, only const methods can be
  // called.
  void ShutdownOnUIThread() override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  void AddObserver(Observer* obs) { observers_.AddObserver(obs); }

  void RemoveObserver(Observer* obs) { observers_.RemoveObserver(obs); }

  // Returns true when the improved cookie control UI should be shown.
  // TODO(dullweber): Fix grammar.
  bool IsCookieControlsEnabled();

 private:
  ~CookieSettings() override;

  // Returns whether third-party cookie blocking should be bypassed (i.e. always
  // allow the cookie regardless of cookie content settings and third-party
  // cookie blocking settings.
  // This just checks the scheme of the |url| and |site_for_cookies|:
  //  - Allow cookies if the |site_for_cookies| is a chrome:// scheme URL, and
  //    the |url| has a secure scheme.
  //  - Allow cookies if the |site_for_cookies| and the |url| match in scheme
  //    and both have the Chrome extensions scheme.
  bool ShouldAlwaysAllowCookies(const GURL& url,
                                const GURL& first_party_url) const;

  // content_settings::CookieSettingsBase:
  void GetCookieSettingInternal(const GURL& url,
                                const GURL& first_party_url,
                                bool is_third_party_request,
                                content_settings::SettingSource* source,
                                ContentSetting* cookie_setting) const override;

  void OnCookiePreferencesChanged();

  base::ThreadChecker thread_checker_;
  base::ObserverList<Observer> observers_;
  scoped_refptr<HostContentSettingsMap> host_content_settings_map_;
  PrefChangeRegistrar pref_change_registrar_;
  const bool is_incognito_;
  const char* extension_scheme_;  // Weak.

  // Used around accesses to |block_third_party_cookies_| to guarantee thread
  // safety.
  mutable base::Lock lock_;

  bool block_third_party_cookies_;

  DISALLOW_COPY_AND_ASSIGN(CookieSettings);
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_COOKIE_SETTINGS_H_
