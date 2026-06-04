// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include "components/prefs/pref_service.h"
#include "components/themes/ntp_background_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"

// static
base::DictValue NtpCustomBackgroundServiceBase::NtpCustomBackgroundDefaults() {
  base::DictValue defaults;
  defaults.Set(kNtpCustomBackgroundURL, base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine1,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionLine2,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundAttributionActionURL,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundCollectionId,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundResumeToken,
               base::Value(base::Value::Type::STRING));
  defaults.Set(kNtpCustomBackgroundRefreshTimestamp,
               base::Value(base::Value::Type::INTEGER));
  return defaults;
}

// static
base::DictValue NtpCustomBackgroundServiceBase::GetBackgroundInfoAsDict(
    const GURL& background_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::optional<std::string>& collection_id,
    const std::optional<std::string>& resume_token,
    std::optional<int> refresh_timestamp) {
  base::DictValue background_info;
  background_info.Set(kNtpCustomBackgroundURL,
                      base::Value(background_url.spec()));
  background_info.Set(kNtpCustomBackgroundAttributionLine1,
                      base::Value(attribution_line_1));
  background_info.Set(kNtpCustomBackgroundAttributionLine2,
                      base::Value(attribution_line_2));
  background_info.Set(kNtpCustomBackgroundAttributionActionURL,
                      base::Value(action_url.spec()));
  background_info.Set(kNtpCustomBackgroundCollectionId,
                      base::Value(collection_id.value_or("")));
  background_info.Set(kNtpCustomBackgroundResumeToken,
                      base::Value(resume_token.value_or("")));
  background_info.Set(kNtpCustomBackgroundRefreshTimestamp,
                      base::Value(refresh_timestamp.value_or(0)));
  return background_info;
}

NtpCustomBackgroundServiceBase::NtpCustomBackgroundServiceBase(
    PrefService* pref_service,
    NtpBackgroundService* background_service)
    : pref_service_(pref_service), background_service_(background_service) {}

NtpCustomBackgroundServiceBase::~NtpCustomBackgroundServiceBase() {
  for (auto& observer : observers_) {
    observer.OnNtpCustomBackgroundServiceShuttingDown();
  }
}

void NtpCustomBackgroundServiceBase::OnCollectionInfoAvailable() {}
void NtpCustomBackgroundServiceBase::OnCollectionImagesAvailable() {}
void NtpCustomBackgroundServiceBase::OnNextCollectionImageAvailable() {}
void NtpCustomBackgroundServiceBase::OnNtpBackgroundServiceShuttingDown() {}

void NtpCustomBackgroundServiceBase::AddObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.AddObserver(observer);
}
void NtpCustomBackgroundServiceBase::RemoveObserver(
    NtpCustomBackgroundServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}
void NtpCustomBackgroundServiceBase::NotifyAboutBackgrounds() {
  for (NtpCustomBackgroundServiceObserver& observer : observers_) {
    observer.OnCustomBackgroundImageUpdated();
  }
}
