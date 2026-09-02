// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/ntp_custom_background_service_base.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_service.h"
#include "components/themes/ntp_background_service.h"
#include "components/themes/ntp_custom_background_service_constants.h"

namespace {

base::DictValue GetBackgroundInfoWithColor(
    const base::DictValue& background_info,
    SkColor color) {
  base::DictValue new_background_info = background_info.Clone();
  new_background_info.Set(kNtpCustomBackgroundMainColor,
                          static_cast<int>(color));
  return new_background_info;
}

}  // namespace

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
    NtpBackgroundService* background_service,
    const std::string& custom_background_dict_pref_name,
    const std::string& local_to_device_pref_name)
    : pref_service_(pref_service),
      background_service_(background_service),
      custom_background_dict_pref_name_(custom_background_dict_pref_name),
      local_to_device_pref_name_(local_to_device_pref_name) {
  if (background_service_) {
    background_service_observation_.Observe(background_service_.get());
  }

  // Update theme info when the pref is changed via Sync.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      custom_background_dict_pref_name_,
      base::BindRepeating(
          &NtpCustomBackgroundServiceBase::UpdateBackgroundFromSync,
          weak_ptr_factory_.GetWeakPtr()));
}

NtpCustomBackgroundServiceBase::~NtpCustomBackgroundServiceBase() {
  for (auto& observer : observers_) {
    observer.OnNtpCustomBackgroundServiceShuttingDown();
  }
}

void NtpCustomBackgroundServiceBase::OnCollectionInfoAvailable() {}
void NtpCustomBackgroundServiceBase::OnCollectionImagesAvailable() {}

void NtpCustomBackgroundServiceBase::OnNextCollectionImageAvailable() {
  auto image = background_service_->next_image();
  std::string attribution1;
  std::string attribution2;
  if (image.attribution.size() > 0)
    attribution1 = image.attribution[0];
  if (image.attribution.size() > 1)
    attribution2 = image.attribution[1];

  std::string resume_token = background_service_->next_image_resume_token();

  base::DictValue background_info = GetBackgroundInfoAsDict(
      image.image_url, attribution1, attribution2, image.attribution_action_url,
      image.collection_id, resume_token, GetNextRefreshTimestamp());

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetDict(custom_background_dict_pref_name_,
                         std::move(background_info));
}

std::optional<int> NtpCustomBackgroundServiceBase::GetNextRefreshTimestamp()
    const {
  return std::nullopt;
}

void NtpCustomBackgroundServiceBase::OnNtpBackgroundServiceShuttingDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  background_service_observation_.Reset();
  background_service_ = nullptr;
}

void NtpCustomBackgroundServiceBase::UpdateBackgroundFromSync() {
  pref_service_->SetBoolean(local_to_device_pref_name_, false);
  NotifyAboutBackgrounds();
}

void NtpCustomBackgroundServiceBase::RefreshBackgroundIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::DictValue& background_info =
      pref_service_->GetDict(custom_background_dict_pref_name_);

  std::string collection_id =
      background_info.Find(kNtpCustomBackgroundCollectionId)->GetString();
  std::string resume_token =
      background_info.Find(kNtpCustomBackgroundResumeToken)->GetString();
  background_service_->FetchNextCollectionImage(collection_id, resume_token);
}

std::optional<CustomBackground>
NtpCustomBackgroundServiceBase::GetCustomBackground() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pref_service_->GetBoolean(local_to_device_pref_name_)) {
    // This should be handled inside each platform.
    return std::nullopt;
  }

  // Attempt to get custom background URL from preferences.
  if (IsCustomBackgroundPrefValid()) {
    auto custom_background = std::make_optional<CustomBackground>();
    const base::DictValue& background_info =
        pref_service_->GetDict(custom_background_dict_pref_name_);
    GURL custom_background_url(
        background_info.Find(kNtpCustomBackgroundURL)->GetString());

    std::string collection_id;
    const base::Value* id_value =
        background_info.Find(kNtpCustomBackgroundCollectionId);
    if (id_value) {
      collection_id = id_value->GetString();
    }

    // Set custom background information in theme info (attributions are
    // optional).
    const base::Value* daily_refresh_timestamp =
        background_info.Find(kNtpCustomBackgroundRefreshTimestamp);
    const base::Value* attribution_line_1 =
        background_info.Find(kNtpCustomBackgroundAttributionLine1);
    const base::Value* attribution_line_2 =
        background_info.Find(kNtpCustomBackgroundAttributionLine2);
    const base::Value* attribution_action_url =
        background_info.Find(kNtpCustomBackgroundAttributionActionURL);
    const base::Value* color =
        background_info.Find(kNtpCustomBackgroundMainColor);
    custom_background->custom_background_url = custom_background_url;
    custom_background->is_uploaded_image = false;
    custom_background->collection_id = collection_id;
    custom_background->daily_refresh_enabled =
        daily_refresh_timestamp && daily_refresh_timestamp->GetInt() != 0;
    if (attribution_line_1) {
      custom_background->custom_background_attribution_line_1 =
          background_info.Find(kNtpCustomBackgroundAttributionLine1)
              ->GetString();
    }
    if (attribution_line_2) {
      custom_background->custom_background_attribution_line_2 =
          background_info.Find(kNtpCustomBackgroundAttributionLine2)
              ->GetString();
    }
    if (attribution_action_url) {
      GURL action_url(
          background_info.Find(kNtpCustomBackgroundAttributionActionURL)
              ->GetString());

      if (!action_url.SchemeIsCryptographic()) {
        custom_background->custom_background_attribution_action_url = GURL();
      } else {
        custom_background->custom_background_attribution_action_url =
            action_url;
      }
    }
    if (color) {
      custom_background->custom_background_main_color =
          static_cast<uint32_t>(color->GetInt());
    }
    return custom_background;
  }

  return std::nullopt;
}

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

void NtpCustomBackgroundServiceBase::ResetCustomBackgroundInfo() {
  SetCustomBackgroundInfo(GURL(), GURL(), std::string(), std::string(), GURL(),
                          std::string());
}

void NtpCustomBackgroundServiceBase::SetCustomBackgroundInfo(
    const GURL& background_url,
    const GURL& thumbnail_url,
    const std::string& attribution_line_1,
    const std::string& attribution_line_2,
    const GURL& action_url,
    const std::string& collection_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool is_backdrop_collection =
      background_service_ &&
      background_service_->IsValidBackdropCollection(collection_id);
  bool is_backdrop_url =
      background_service_ &&
      background_service_->IsValidBackdropUrl(background_url);

  bool need_forced_refresh =
      pref_service_->GetBoolean(local_to_device_pref_name_) &&
      pref_service_->FindPreference(custom_background_dict_pref_name_)
          ->IsDefaultValue();

  pref_service_->SetBoolean(local_to_device_pref_name_, false);

  if (!background_url.is_valid() && !collection_id.empty() &&
      is_backdrop_collection) {
    background_service_->FetchNextCollectionImage(collection_id, std::nullopt);
  } else if (background_url.is_valid() && is_backdrop_url) {
    base::DictValue background_info = GetBackgroundInfoAsDict(
        background_url, attribution_line_1, attribution_line_2, action_url,
        collection_id, std::nullopt, std::nullopt);
    pref_service_->SetDict(custom_background_dict_pref_name_,
                           std::move(background_info));
  } else {
    pref_service_->ClearPref(custom_background_dict_pref_name_);

    // If this device was using a local image and did not have a non-local
    // background saved, UpdateBackgroundFromSync will not fire. Therefore, we
    // need to force a refresh here.
    if (need_forced_refresh) {
      NotifyAboutBackgrounds();
    }
  }
}

bool NtpCustomBackgroundServiceBase::IsCustomBackgroundPrefValid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const base::DictValue& background_info =
      pref_service_->GetDict(custom_background_dict_pref_name_);

  const base::Value* background_url =
      background_info.Find(kNtpCustomBackgroundURL);
  if (!background_url) {
    return false;
  }

  return GURL(background_url->GetString()).is_valid();
}

bool NtpCustomBackgroundServiceBase::UpdateCustomBackgroundPrefsWithColor(
    const GURL& image_url,
    SkColor color) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Update background color only if the selected background is still the same.
  const base::DictValue& background_info =
      pref_service_->GetDict(custom_background_dict_pref_name_);
  const std::string* current_bg_url =
      background_info.FindString(kNtpCustomBackgroundURL);
  if (!current_bg_url || GURL(*current_bg_url) != image_url) {
    return false;
  }
  pref_service_->SetDict(custom_background_dict_pref_name_,
                         GetBackgroundInfoWithColor(background_info, color));
  return true;
}
