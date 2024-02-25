// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/content/clipboard_restriction_service.h"

#include "components/enterprise/content/pref_names.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_matcher/url_util.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"

ClipboardRestrictionService::ClipboardRestrictionService(
    PrefService* pref_service)
    : pref_service_(pref_service),
      next_id_(0),
      enable_url_matcher_(nullptr),
      disable_url_matcher_(nullptr) {
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      enterprise::content::kCopyPreventionSettings,
      base::BindRepeating(&ClipboardRestrictionService::UpdateSettings,
                          base::Unretained(this)));
  UpdateSettings();
}

ClipboardRestrictionService::~ClipboardRestrictionService() = default;

bool ClipboardRestrictionService::IsUrlAllowedToCopy(
    const GURL& url,
    size_t data_size_in_bytes,
    std::u16string* replacement_data) const {
  if (!enable_url_matcher_ || !disable_url_matcher_)
    return true;

  if (data_size_in_bytes < min_data_size_)
    return true;

  // The copy is allowed in the following scenarios:
  // 1. The URL doesn't match a pattern in the enable list
  // or
  // 2. The URL matches a pattern in the enable list but also matches a pattern
  //    in the disable list
  // Conversely, it is blocked iff it matches a pattern in the enable list and
  // doesn't match a pattern in the disable list.
  bool is_allowed = enable_url_matcher_->MatchURL(url).empty() ||
                    disable_url_matcher_->MatchURL(url).size() > 0;

  if (!is_allowed && replacement_data) {
    *replacement_data = l10n_util::GetStringUTF16(
        IDS_ENTERPRISE_COPY_PREVENTION_WARNING_MESSAGE);
  }

  return is_allowed;
}

void ClipboardRestrictionService::UpdateSettings() {
  // This is infrequent enough that this code can teardown the url matcher
  // entirely and rebuild it. There's also no state that should be carried over
  // from the previous values, so doing it this way makes that semantic clearer.
  enable_url_matcher_ = nullptr;
  disable_url_matcher_ = nullptr;

  if (!pref_service_->IsManagedPreference(
          enterprise::content::kCopyPreventionSettings)) {
    return;
  }

  const base::Value::Dict& settings =
      pref_service_->GetDict(enterprise::content::kCopyPreventionSettings);
  const base::Value::List* enable = settings.FindList(
      enterprise::content::kCopyPreventionSettingsEnableFieldName);
  const base::Value::List* disable = settings.FindList(
      enterprise::content::kCopyPreventionSettingsDisableFieldName);

  DCHECK(enable);
  DCHECK(disable);

  enable_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  disable_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();

  // For the following 2 calls, the second param is a bool called `allow`. In
  // this context, we're not concerned about a URL being "allowed" or not, but
  // rather with this prevention feature being enabled on a given URL. Because
  // of this, pass `true` for patterns in the `enable` list and false for
  // patterns in the `disable` list. If the URL Matcher subsequently matches a
  // URL as "allowed", it means the prevention feature is active for that URL
  // and the copy will be blocked. While confusing, this is mostly to map to the
  // same policy format as the content analysis connector, which also has
  // "enable" and "disable" lists used in this way.
  url_matcher::util::AddFilters(enable_url_matcher_.get(), true, &next_id_,
                                *enable);
  url_matcher::util::AddFilters(disable_url_matcher_.get(), false, &next_id_,
                                *disable);

  std::optional<int> min_data_size = settings.FindInt(
      enterprise::content::kCopyPreventionSettingsMinDataSizeFieldName);
  DCHECK(min_data_size);
  DCHECK(min_data_size >= 0);
  min_data_size_ = *min_data_size;
}

// static
ClipboardRestrictionServiceFactory*
ClipboardRestrictionServiceFactory::GetInstance() {
  return base::Singleton<ClipboardRestrictionServiceFactory>::get();
}

// static
ClipboardRestrictionService*
ClipboardRestrictionServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ClipboardRestrictionService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ClipboardRestrictionServiceFactory::ClipboardRestrictionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PolicyClipboardRestriction",
          BrowserContextDependencyManager::GetInstance()) {}

ClipboardRestrictionServiceFactory::~ClipboardRestrictionServiceFactory() =
    default;

content::BrowserContext*
ClipboardRestrictionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

KeyedService* ClipboardRestrictionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ClipboardRestrictionService(user_prefs::UserPrefs::Get(context));
}
