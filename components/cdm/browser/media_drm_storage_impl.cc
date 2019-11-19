// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cdm/browser/media_drm_storage_impl.h"

#include <map>
#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/value_conversions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/navigation_handle.h"
#include "media/base/android/media_drm_key_type.h"
#include "url/origin.h"
#include "url/url_constants.h"

// The storage will be managed by PrefService. All data will be stored in a
// dictionary under the key "media.media_drm_storage". The dictionary is
// structured as follows:
//
// {
//     $origin: {
//         "origin_id": $unguessable_origin_id
//         "creation_time": $creation_time
//         "sessions" : {
//             $session_id: {
//                 "key_set_id": $key_set_id,
//                 "mime_type": $mime_type,
//                 "creation_time": $creation_time,
//                 "key_type": $key_type (enum of MediaDrmKeyType)
//             },
//             # more session_id map...
//         }
//     },
//     # more origin map...
// }
//
// "creation_time" is the UTC time when "origin_id" was generated. When
// provisioning for this origin completes, "creation_time" is updated to the UTC
// time at which that occured. Since the value is only used to optimize the
// clear media licenses process, and in practice the difference between the two
// values is small, it's OK to replace the generation time with completion time.

namespace cdm {

namespace prefs {

const char kMediaDrmStorage[] = "media.media_drm_storage";

}  // namespace prefs

namespace {

const char kCreationTime[] = "creation_time";
const char kSessions[] = "sessions";
const char kKeySetId[] = "key_set_id";
const char kMimeType[] = "mime_type";
const char kKeyType[] = "key_type";
const char kOriginId[] = "origin_id";

bool GetMediaDrmKeyTypeFromDict(const base::Value& dict,
                                media::MediaDrmKeyType* value_out) {
  DCHECK(dict.is_dict());
  DCHECK(value_out);

  const base::Value* value =
      dict.FindKeyOfType(kKeyType, base::Value::Type::INTEGER);
  if (!value)
    return false;

  int key_type = value->GetInt();
  if (key_type < static_cast<int>(media::MediaDrmKeyType::UNKNOWN) ||
      key_type > static_cast<int>(media::MediaDrmKeyType::MAX)) {
    DVLOG(1) << "Corrupted key type.";
    return false;
  }

  *value_out = static_cast<media::MediaDrmKeyType>(key_type);
  return true;
}

bool GetStringFromDict(const base::Value& dict,
                       const std::string& key,
                       std::string* value_out) {
  DCHECK(dict.is_dict());
  DCHECK(value_out);

  const base::Value* value = dict.FindKeyOfType(key, base::Value::Type::STRING);
  if (!value)
    return false;

  *value_out = value->GetString();
  return true;
}

// Extract base::Time from |dict| with key kCreationTime. Returns true if |dict|
// contains a valid time value.
bool GetCreationTimeFromDict(const base::Value& dict, base::Time* time) {
  DCHECK(dict.is_dict());
  DCHECK(time);

  const base::Value* time_value =
      dict.FindKeyOfType(kCreationTime, base::Value::Type::DOUBLE);
  if (!time_value)
    return false;

  base::Time time_maybe_null = base::Time::FromDoubleT(time_value->GetDouble());
  if (time_maybe_null.is_null())
    return false;

  *time = time_maybe_null;
  return true;
}

// Data in origin dict without sessions dict.
class OriginData {
 public:
  explicit OriginData(const base::UnguessableToken& origin_id)
      : OriginData(origin_id, base::Time::Now()) {
    DCHECK(origin_id_);
  }

  const base::UnguessableToken& origin_id() const { return origin_id_; }

  base::Time provision_time() const { return provision_time_; }

  base::Value ToDictValue() const {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetKey(kOriginId, base::CreateUnguessableTokenValue(origin_id_));
    dict.SetKey(kCreationTime, base::Value(provision_time_.ToDoubleT()));

    return dict;
  }

  // Convert |origin_dict| to OriginData. |origin_dict| contains information
  // related to origin provision. Return nullptr if |origin_dict| has any
  // corruption, e.g. format error, missing fields, invalid value.
  static std::unique_ptr<OriginData> FromDictValue(
      const base::Value& origin_dict) {
    DCHECK(origin_dict.is_dict());

    const base::Value* origin_id_value =
        origin_dict.FindKeyOfType(kOriginId, base::Value::Type::STRING);
    if (!origin_id_value)
      return nullptr;

    base::UnguessableToken origin_id;
    if (!base::GetValueAsUnguessableToken(*origin_id_value, &origin_id))
      return nullptr;

    base::Time time;
    if (!GetCreationTimeFromDict(origin_dict, &time))
      return nullptr;

    return base::WrapUnique(new OriginData(origin_id, time));
  }

 private:
  OriginData(const base::UnguessableToken& origin_id, base::Time time)
      : origin_id_(origin_id), provision_time_(time) {}

  base::UnguessableToken origin_id_;
  base::Time provision_time_;
};

// Data in session dict.
class SessionData {
 public:
  SessionData(const std::vector<uint8_t>& key_set_id,
              const std::string& mime_type,
              media::MediaDrmKeyType key_type)
      : SessionData(key_set_id, mime_type, key_type, base::Time::Now()) {}

  base::Time creation_time() const { return creation_time_; }

  base::Value ToDictValue() const {
    base::Value dict(base::Value::Type::DICTIONARY);

    dict.SetKey(kKeySetId,
                base::Value(std::string(
                    reinterpret_cast<const char*>(key_set_id_.data()),
                    key_set_id_.size())));
    dict.SetKey(kMimeType, base::Value(mime_type_));
    dict.SetKey(kKeyType, base::Value(static_cast<int>(key_type_)));
    dict.SetKey(kCreationTime, base::Value(creation_time_.ToDoubleT()));

    return dict;
  }

  media::mojom::SessionDataPtr ToMojo() const {
    return media::mojom::SessionData::New(key_set_id_, mime_type_, key_type_);
  }

  // Convert |session_dict| to SessionData. |session_dict| contains information
  // for an offline license session. Return nullptr if |session_dict| has any
  // corruption, e.g. format error, missing fields, invalid data.
  static std::unique_ptr<SessionData> FromDictValue(
      const base::Value& session_dict) {
    DCHECK(session_dict.is_dict());

    std::string key_set_id_string;
    if (!GetStringFromDict(session_dict, kKeySetId, &key_set_id_string))
      return nullptr;

    std::string mime_type;
    if (!GetStringFromDict(session_dict, kMimeType, &mime_type))
      return nullptr;

    base::Time time;
    if (!GetCreationTimeFromDict(session_dict, &time))
      return nullptr;

    media::MediaDrmKeyType key_type;
    if (!GetMediaDrmKeyTypeFromDict(session_dict, &key_type)) {
      DVLOG(1) << "Missing key type.";
      key_type = media::MediaDrmKeyType::UNKNOWN;
    }

    return base::WrapUnique(
        new SessionData(std::vector<uint8_t>(key_set_id_string.begin(),
                                             key_set_id_string.end()),
                        std::move(mime_type), key_type, time));
  }

 private:
  SessionData(std::vector<uint8_t> key_set_id,
              std::string mime_type,
              media::MediaDrmKeyType key_type,
              base::Time time)
      : key_set_id_(std::move(key_set_id)),
        mime_type_(std::move(mime_type)),
        key_type_(key_type),
        creation_time_(time) {}

  std::vector<uint8_t> key_set_id_;
  std::string mime_type_;
  media::MediaDrmKeyType key_type_;
  base::Time creation_time_;
};

// Get sessions dict for |origin_string| in |storage_dict|. This is a helper
// function that works for both const and non const access.
template <typename DictValue>
DictValue* GetSessionsDictFromStorageDict(DictValue* storage_dict,
                                          const std::string& origin_string) {
  if (storage_dict == nullptr) {
    DVLOG(1) << __func__ << ": No storage dict for origin " << origin_string;
    return nullptr;
  }

  DCHECK(storage_dict->is_dict());

  DictValue* origin_dict =
      storage_dict->FindKeyOfType(origin_string, base::Value::Type::DICTIONARY);
  if (!origin_dict) {
    DVLOG(1) << __func__ << ": No entry for origin " << origin_string;
    return nullptr;
  }

  DictValue* sessions_dict =
      origin_dict->FindKeyOfType(kSessions, base::Value::Type::DICTIONARY);
  if (!sessions_dict) {
    DVLOG(1) << __func__ << ": No sessions entry for origin " << origin_string;
    return nullptr;
  }

  return sessions_dict;
}

// Create origin dict with |origin_id|, current time as creation time, and empty
// sessions dict. It returns the sessions dict for caller to write session
// information. Note that this clears any existing session information.
base::Value* CreateOriginDictAndReturnSessionsDict(
    base::Value* storage_dict,
    const url::Origin& origin,
    const base::UnguessableToken& origin_id) {
  DCHECK(storage_dict);
  DCHECK(storage_dict->is_dict());

  return storage_dict
      ->SetKey(origin.Serialize(), OriginData(origin_id).ToDictValue())
      ->SetKey(kSessions, base::Value(base::Value::Type::DICTIONARY));
}

// Clear sessions whose creation time falls in [start, end] from
// |sessions_dict|. This function also cleans corruption data and should never
// fail.
void ClearSessionDataForTimePeriod(base::Value* sessions_dict,
                                   base::Time start,
                                   base::Time end) {
  DCHECK(sessions_dict->is_dict());

  std::vector<std::string> sessions_to_clear;
  for (const auto& key_value : sessions_dict->DictItems()) {
    const std::string& session_id = key_value.first;

    base::Value* session_dict = &key_value.second;
    if (!session_dict->is_dict()) {
      DLOG(WARNING) << "Session dict for " << session_id
                    << " is corrupted, removing.";
      sessions_to_clear.push_back(session_id);
      continue;
    }

    std::unique_ptr<SessionData> session_data =
        SessionData::FromDictValue(*session_dict);
    if (!session_data) {
      DLOG(WARNING) << "Session data for " << session_id
                    << " is corrupted, removing.";
      sessions_to_clear.push_back(session_id);
      continue;
    }

    if (session_data->creation_time() >= start &&
        session_data->creation_time() <= end) {
      sessions_to_clear.push_back(session_id);
      continue;
    }
  }

  // Remove session data.
  for (const auto& session_id : sessions_to_clear)
    sessions_dict->RemoveKey(session_id);
}

// 1. Removes the session data from origin dict if the session's creation time
// falls in [|start|, |end|] and |filter| returns true on its origin.
// 2. Removes the origin data if all of the sessions are removed.
// 3. Returns a list of origin IDs to unprovision.
std::vector<base::UnguessableToken> ClearMatchingLicenseData(
    base::DictionaryValue* storage_dict,
    base::Time start,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& filter) {
  std::vector<std::string> origins_to_delete;
  std::vector<base::UnguessableToken> origin_ids_to_unprovision;

  for (const auto& key_value : storage_dict->DictItems()) {
    const std::string& origin_str = key_value.first;

    if (filter && !filter.Run(GURL(origin_str)))
      continue;

    base::Value* origin_dict = &key_value.second;
    if (!origin_dict->is_dict()) {
      DLOG(WARNING) << "Origin dict for " << origin_str
                    << " is corrupted, removing.";
      origins_to_delete.push_back(origin_str);
      continue;
    }

    std::unique_ptr<OriginData> origin_data =
        OriginData::FromDictValue(*origin_dict);
    if (!origin_data) {
      DLOG(WARNING) << "Origin data for " << origin_str
                    << " is corrupted, removing.";
      origins_to_delete.push_back(origin_str);
      continue;
    }

    if (origin_data->provision_time() > end)
      continue;

    base::Value* sessions =
        origin_dict->FindKeyOfType(kSessions, base::Value::Type::DICTIONARY);
    if (!sessions) {
      // The origin is provisioned, but no persistent license is installed.
      origins_to_delete.push_back(origin_str);
      origin_ids_to_unprovision.push_back(origin_data->origin_id());
      continue;
    }

    ClearSessionDataForTimePeriod(sessions, start, end);

    if (sessions->DictEmpty()) {
      // Session data will be removed when removing origin data.
      origins_to_delete.push_back(origin_str);
      origin_ids_to_unprovision.push_back(origin_data->origin_id());
    }
  }

  // Remove origin data.
  for (const auto& origin_str : origins_to_delete)
    storage_dict->RemoveKey(origin_str);

  return origin_ids_to_unprovision;
}

// Returns true if any session in |sessions_dict| has been modified more
// recently than |modified_since|, and otherwise returns false.
bool SessionsModifiedSince(const base::Value* sessions_dict,
                           base::Time modified_since) {
  DCHECK(sessions_dict->is_dict());
  for (const auto& key_value : sessions_dict->DictItems()) {
    const base::Value* session_dict = &key_value.second;
    if (!session_dict->is_dict())
      continue;

    std::unique_ptr<SessionData> session_data =
        SessionData::FromDictValue(*session_dict);
    if (!session_data)
      continue;

    if (session_data->creation_time() >= modified_since)
      return true;
  }

  // No session creation time >= |modified_since|.
  return false;
}

// Returns the origin ID for |origin|, if it exists. Will return an empty value
// if the origin ID can not be found in |storage_dict|.
base::UnguessableToken GetOriginIdForOrigin(
    const base::DictionaryValue* storage_dict,
    const url::Origin& origin) {
  DCHECK(storage_dict);

  const base::Value* origin_dict = storage_dict->FindKeyOfType(
      origin.Serialize(), base::Value::Type::DICTIONARY);
  if (!origin_dict)
    return base::UnguessableToken::Null();

  std::unique_ptr<OriginData> origin_data =
      OriginData::FromDictValue(*origin_dict);
  if (!origin_data)
    return base::UnguessableToken::Null();

  // |origin_id| can be empty even if |origin_dict| exists. This can happen
  // if |origin_dict| was created with an old version of Chrome.
  return origin_data->origin_id();
}

// Map shared by all MediaDrmStorageImpl objects to prevent multiple Origin IDs
// being allocated for the same origin. Initialize() can be called multiple
// times for the same web origin (and profile). This class groups together
// requests for the same web origin (and profile), calling |get_origin_id_cb|
// only on the first request. This avoids a race condition where simultaneous
// requests could result in different Origin IDs allocated for the same origin
// (and only the last one saved in the preference).
class InitializationSerializer {
 public:
  // Origin IDs are unique per preference service and origin.
  struct PreferenceAndOriginKey {
    // Allow use as a key in std::map.
    bool operator<(const PreferenceAndOriginKey& other) const {
      return std::tie(pref_service, origin) <
             std::tie(other.pref_service, other.origin);
    }

    PrefService* pref_service;
    const url::Origin origin;
  };

  // The InitializationSerializer is a global map shared by all
  // MediaDrmStorageImpl instances.
  static InitializationSerializer& GetInstance() {
    static base::NoDestructor<InitializationSerializer>
        s_origin_id_request_impl;
    return *s_origin_id_request_impl;
  }

  InitializationSerializer() = default;
  ~InitializationSerializer() = default;

  void FetchOriginId(
      PrefService* pref_service,
      const url::Origin& origin,
      MediaDrmStorageImpl::GetOriginIdCB get_origin_id_cb,
      MediaDrmStorageImpl::OriginIdObtainedCB origin_id_obtained_cb) {
    DVLOG(3) << __func__ << " origin: " << origin;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Check if the preference has an existing origin ID.
    const base::DictionaryValue* storage_dict =
        pref_service->GetDictionary(prefs::kMediaDrmStorage);
    base::UnguessableToken origin_id =
        GetOriginIdForOrigin(storage_dict, origin);
    if (origin_id) {
      DVLOG(3) << __func__
               << ": Found origin ID in pref service dictionary: " << origin_id;
      std::move(origin_id_obtained_cb).Run(true, origin_id);
      return;
    }

    // No origin ID found, so check if another Initialize() call is in progress.
    PreferenceAndOriginKey key{pref_service, origin};
    auto entry = pending_requests_.find(key);
    if (entry != pending_requests_.end()) {
      // Entry already exists, so simply add |origin_id_obtained_cb| to be
      // called once the Origin ID is obtained.
      entry->second.emplace_back(std::move(origin_id_obtained_cb));
      return;
    }

    // Entry does not exist, so create a new one.
    std::vector<MediaDrmStorageImpl::OriginIdObtainedCB>
        origin_id_obtained_cb_list;
    origin_id_obtained_cb_list.emplace_back(std::move(origin_id_obtained_cb));
    pending_requests_.emplace(key, std::move(origin_id_obtained_cb_list));

    // Now call |get_origin_id_cb|. It will call OnOriginIdObtained() when done,
    // which will call all the callbacks saved for this preference and origin
    // pair. Use of base::Unretained() is valid as |this| is a singleton stored
    // in a static variable.
    DVLOG(3) << __func__ << ": Call |get_origin_id_cb| to get origin ID.";
    get_origin_id_cb.Run(
        base::BindOnce(&InitializationSerializer::OnOriginIdObtained,
                       base::Unretained(this), pref_service, origin));
  }

 private:
  void OnOriginIdObtained(
      PrefService* pref_service,
      const url::Origin& origin,
      bool success,
      const MediaDrmStorageImpl::MediaDrmOriginId& origin_id) {
    DVLOG(3) << __func__ << " origin: " << origin;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // Save the origin ID in the preference as long as it is not null.
    if (origin_id) {
      DictionaryPrefUpdate update(pref_service, prefs::kMediaDrmStorage);
      CreateOriginDictAndReturnSessionsDict(update.Get(), origin,
                                            origin_id.value());
    }

    // Now call any callbacks waiting for this origin ID to be allocated.
    auto entry = pending_requests_.find({pref_service, origin});
    DCHECK(entry != pending_requests_.end());

    std::vector<MediaDrmStorageImpl::OriginIdObtainedCB> callbacks;
    callbacks.swap(entry->second);
    pending_requests_.erase(entry);

    for (auto& callback : callbacks)
      std::move(callback).Run(success, origin_id);
  }

  // Note that this map is never deleted. As entries are removed when an origin
  // ID is allocated, so it should never get too large.
  std::map<PreferenceAndOriginKey,
           std::vector<MediaDrmStorageImpl::OriginIdObtainedCB>>
      pending_requests_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(InitializationSerializer);
};

}  // namespace

// static
void MediaDrmStorageImpl::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMediaDrmStorage);
}

// static
std::set<GURL> MediaDrmStorageImpl::GetAllOrigins(
    const PrefService* pref_service) {
  DCHECK(pref_service);

  const base::DictionaryValue* storage_dict =
      pref_service->GetDictionary(prefs::kMediaDrmStorage);
  if (!storage_dict)
    return std::set<GURL>();

  std::set<GURL> origin_set;
  for (const auto& key_value : *storage_dict) {
    GURL origin(key_value.first);
    if (origin.is_valid())
      origin_set.insert(origin);
  }

  return origin_set;
}

// static
std::vector<GURL> MediaDrmStorageImpl::GetOriginsModifiedSince(
    const PrefService* pref_service,
    base::Time modified_since) {
  DCHECK(pref_service);

  const base::DictionaryValue* storage_dict =
      pref_service->GetDictionary(prefs::kMediaDrmStorage);
  if (!storage_dict)
    return {};

  // Check each origin to see if it has been modified since |modified_since|.
  // If there are any errors in prefs::kMediaDrmStorage, ignore them.
  std::vector<GURL> matching_origins;
  for (const auto& key_value : storage_dict->DictItems()) {
    GURL origin(key_value.first);
    if (!origin.is_valid())
      continue;

    const base::Value* origin_dict = &key_value.second;
    if (!origin_dict->is_dict())
      continue;

    std::unique_ptr<OriginData> origin_data =
        OriginData::FromDictValue(*origin_dict);
    if (!origin_data)
      continue;

    // There is no need to check the sessions if the origin was provisioned
    // after |modified_since|.
    if (origin_data->provision_time() < modified_since) {
      // See if any session created recently.
      const base::Value* sessions =
          origin_dict->FindKeyOfType(kSessions, base::Value::Type::DICTIONARY);
      if (!sessions)
        continue;

      // If no sessions modified recently, move on to the next origin.
      if (!SessionsModifiedSince(sessions, modified_since))
        continue;
    }

    // Either the origin has been provisioned after |modified_since| or there
    // are sessions created after |modified_since|, so add the origin to the
    // list returned.
    matching_origins.push_back(origin);
  }

  return matching_origins;
}

// static
std::vector<base::UnguessableToken> MediaDrmStorageImpl::ClearMatchingLicenses(
    PrefService* pref_service,
    base::Time start,
    base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& filter) {
  DVLOG(1) << __func__ << ": Clear licenses [" << start << ", " << end << "]";

  DictionaryPrefUpdate update(pref_service, prefs::kMediaDrmStorage);

  return ClearMatchingLicenseData(update.Get(), start, end, filter);
}

// MediaDrmStorageImpl

MediaDrmStorageImpl::MediaDrmStorageImpl(
    content::RenderFrameHost* render_frame_host,
    PrefService* pref_service,
    GetOriginIdCB get_origin_id_cb,
    AllowEmptyOriginIdCB allow_empty_origin_id_cb,
    mojo::PendingReceiver<media::mojom::MediaDrmStorage> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)),
      pref_service_(pref_service),
      get_origin_id_cb_(get_origin_id_cb),
      allow_empty_origin_id_cb_(allow_empty_origin_id_cb) {
  DVLOG(1) << __func__ << ": origin = " << origin();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(pref_service_);
  DCHECK(get_origin_id_cb_);
  DCHECK(allow_empty_origin_id_cb_);
  DCHECK(!origin().opaque());
}

MediaDrmStorageImpl::~MediaDrmStorageImpl() {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (init_cb_)
    std::move(init_cb_).Run(false, base::nullopt);
}

void MediaDrmStorageImpl::Initialize(InitializeCallback callback) {
  DVLOG(1) << __func__ << ": origin = " << origin();
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!init_cb_);

  if (is_initialized_) {
    std::move(callback).Run(true, origin_id_);
    return;
  }
  DCHECK(!origin_id_);

  // Call using SerializeInitializeRequests() to handle the case where
  // Initialize() is called concurrently for the same origin (and preference
  // service). Note that if there is already an existing origin ID for this web
  // origin saved in the preference, OnOriginIdObtained() will be called
  // immediately.
  init_cb_ = std::move(callback);
  InitializationSerializer::GetInstance().FetchOriginId(
      pref_service_, origin(), get_origin_id_cb_,
      base::BindOnce(&MediaDrmStorageImpl::OnOriginIdObtained,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmStorageImpl::OnOriginIdObtained(
    bool success,
    const MediaDrmOriginId& origin_id) {
  is_initialized_ = true;

  if (!success) {
    // Unable to get a pre-provisioned origin ID, so call
    // |allow_empty_origin_id_cb_| to see if the empty origin ID is allowed.
    allow_empty_origin_id_cb_.Run(
        base::BindOnce(&MediaDrmStorageImpl::OnEmptyOriginIdAllowed,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  origin_id_ = origin_id;
  std::move(init_cb_).Run(success, origin_id_);
}

void MediaDrmStorageImpl::OnEmptyOriginIdAllowed(bool allowed) {
  std::move(init_cb_).Run(allowed, base::nullopt);
}

void MediaDrmStorageImpl::OnProvisioned(OnProvisionedCallback callback) {
  DVLOG(1) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    std::move(callback).Run(false);
    return;
  }

  // If this is using an empty origin ID, it should not be provisioned.
  if (!origin_id_) {
    DVLOG(1) << __func__ << ": Empty origin ID.";
    std::move(callback).Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, prefs::kMediaDrmStorage);
  base::DictionaryValue* storage_dict = update.Get();
  DCHECK(storage_dict);

  // Update origin dict once origin provisioning completes. There may be
  // orphaned session info from a previous provisioning. Clear them by
  // recreating the dicts.
  CreateOriginDictAndReturnSessionsDict(storage_dict, origin(),
                                        origin_id_.value());
  std::move(callback).Run(true);
}

void MediaDrmStorageImpl::SavePersistentSession(
    const std::string& session_id,
    media::mojom::SessionDataPtr session_data,
    SavePersistentSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    std::move(callback).Run(false);
    return;
  }

  // If this is using an empty origin ID, it cannot save persistent data.
  if (!origin_id_) {
    DVLOG(1) << __func__ << ": Empty origin ID.";
    std::move(callback).Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, prefs::kMediaDrmStorage);
  base::DictionaryValue* storage_dict = update.Get();
  DCHECK(storage_dict);

  base::Value* sessions_dict = GetSessionsDictFromStorageDict<base::Value>(
      storage_dict, origin().Serialize());

  // This could happen if the profile is removed, but the device is still
  // provisioned for the origin. In this case, just create a new entry.
  // Since we're using random origin ID in MediaDrm, it's rare to enter the if
  // branch. Deleting the profile causes reprovisioning of the origin.
  if (!sessions_dict) {
    DVLOG(1) << __func__ << ": No entry for origin " << origin();
    sessions_dict = CreateOriginDictAndReturnSessionsDict(
        storage_dict, origin(), origin_id_.value());
    DCHECK(sessions_dict);
  }

  DVLOG_IF(1, sessions_dict->FindKey(session_id))
      << __func__ << ": Session ID already exists and will be replaced.";

  // The key type of the session should be valid. MediaDrmKeyType::MIN/UNKNOWN
  // is an invalid type and caller should never pass it here.
  DCHECK_GT(session_data->key_type, media::MediaDrmKeyType::MIN);
  DCHECK_LE(session_data->key_type, media::MediaDrmKeyType::MAX);

  sessions_dict->SetKey(
      session_id, SessionData(session_data->key_set_id, session_data->mime_type,
                              session_data->key_type)
                      .ToDictValue());

  std::move(callback).Run(true);
}

void MediaDrmStorageImpl::LoadPersistentSession(
    const std::string& session_id,
    LoadPersistentSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    std::move(callback).Run(nullptr);
    return;
  }

  // If this is using an empty origin ID, it cannot save persistent data.
  if (!origin_id_) {
    DVLOG(1) << __func__ << ": Empty origin ID.";
    std::move(callback).Run(nullptr);
    return;
  }

  const base::Value* sessions_dict =
      GetSessionsDictFromStorageDict<const base::Value>(
          pref_service_->GetDictionary(prefs::kMediaDrmStorage),
          origin().Serialize());
  if (!sessions_dict) {
    std::move(callback).Run(nullptr);
    return;
  }

  const base::Value* session_dict =
      sessions_dict->FindKeyOfType(session_id, base::Value::Type::DICTIONARY);
  if (!session_dict) {
    DVLOG(1) << __func__ << ": No session " << session_id << " for origin "
             << origin();
    std::move(callback).Run(nullptr);
    return;
  }

  std::unique_ptr<SessionData> session_data =
      SessionData::FromDictValue(*session_dict);
  if (!session_data) {
    DLOG(WARNING) << __func__ << ": Failed to read session data.";
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(session_data->ToMojo());
}

void MediaDrmStorageImpl::RemovePersistentSession(
    const std::string& session_id,
    RemovePersistentSessionCallback callback) {
  DVLOG(2) << __func__;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!is_initialized_) {
    DVLOG(1) << __func__ << ": Not initialized.";
    std::move(callback).Run(false);
    return;
  }

  // If this is using an empty origin ID, it cannot save persistent data.
  if (!origin_id_) {
    DVLOG(1) << __func__ << ": Empty origin ID.";
    std::move(callback).Run(false);
    return;
  }

  DictionaryPrefUpdate update(pref_service_, prefs::kMediaDrmStorage);

  base::Value* sessions_dict = GetSessionsDictFromStorageDict<base::Value>(
      update.Get(), origin().Serialize());

  if (!sessions_dict) {
    std::move(callback).Run(true);
    return;
  }

  sessions_dict->RemoveKey(session_id);
  std::move(callback).Run(true);
}

}  // namespace cdm
