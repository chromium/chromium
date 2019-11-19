// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/invalidation_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "components/invalidation/impl/jni_headers/InvalidationService_jni.h"
#include "components/invalidation/public/object_id_invalidation_map.h"
#include "google/cacheinvalidation/types.pb.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::JavaParamRef;

namespace invalidation {

InvalidationServiceAndroid::InvalidationServiceAndroid()
    : invalidator_state_(syncer::INVALIDATIONS_ENABLED), logger_() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> local_java_ref =
      Java_InvalidationService_create(env, reinterpret_cast<intptr_t>(this));
  java_ref_.Reset(env, local_java_ref.obj());
  logger_.OnStateChange(invalidator_state_);
}

InvalidationServiceAndroid::~InvalidationServiceAndroid() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void InvalidationServiceAndroid::RegisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.RegisterHandler(handler);
  logger_.OnRegistration(handler->GetOwnerName());
}

bool InvalidationServiceAndroid::UpdateRegisteredInvalidationIds(
    syncer::InvalidationHandler* handler,
    const syncer::ObjectIdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  if (!invalidator_registrar_.UpdateRegisteredIds(handler, ids))
    return false;
  const syncer::ObjectIdSet& registered_ids =
      invalidator_registrar_.GetAllRegisteredIds();

  // To call the corresponding method on the Java invalidation service, split
  // the object ids into object source and object name arrays.
  std::vector<int> sources;
  std::vector<std::string> names;
  syncer::ObjectIdSet::const_iterator id;
  for (id = registered_ids.begin(); id != registered_ids.end(); ++id) {
    sources.push_back(id->source());
    names.push_back(id->name());
  }

  Java_InvalidationService_setRegisteredObjectIds(
      env, java_ref_, base::android::ToJavaIntArray(env, sources),
      base::android::ToJavaArrayOfStrings(env, names));

  logger_.OnUpdateIds(invalidator_registrar_.GetSanitizedHandlersIdsMap());
  return true;
}

void InvalidationServiceAndroid::UnregisterInvalidationHandler(
    syncer::InvalidationHandler* handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_registrar_.UnregisterHandler(handler);
  logger_.OnUnregistration(handler->GetOwnerName());
}

syncer::InvalidatorState
InvalidationServiceAndroid::GetInvalidatorState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return invalidator_state_;
}

std::string InvalidationServiceAndroid::GetInvalidatorClientId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  JNIEnv* env = base::android::AttachCurrentThread();
  DCHECK(env);

  // Ask the Java code to for the invalidator ID it's currently using.
  base::android::ScopedJavaLocalRef<_jbyteArray*> id_bytes_java =
      Java_InvalidationService_getInvalidatorClientId(env, java_ref_);

  // Convert it into a more convenient format for C++.
  std::string id;
  base::android::JavaByteArrayToString(env, id_bytes_java, &id);

  return id;
}

InvalidationLogger* InvalidationServiceAndroid::GetInvalidationLogger() {
  return &logger_;
}

void InvalidationServiceAndroid::RequestDetailedStatus(
    base::Callback<void(const base::DictionaryValue&)> return_callback) const {
}

void InvalidationServiceAndroid::TriggerStateChangeForTest(
    syncer::InvalidatorState state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  invalidator_state_ = state;
  invalidator_registrar_.UpdateInvalidatorState(invalidator_state_);
}

void InvalidationServiceAndroid::Invalidate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint object_source,
    const JavaParamRef<jstring>& java_object_id,
    jlong version,
    const JavaParamRef<jstring>& java_payload) {
  syncer::ObjectIdInvalidationMap object_invalidation_map;
  if (!java_object_id) {
    syncer::ObjectIdSet sync_ids;
    if (object_source == 0) {
      sync_ids = invalidator_registrar_.GetAllRegisteredIds();
    } else {
      for (const auto& id : invalidator_registrar_.GetAllRegisteredIds()) {
        if (id.source() == object_source)
          sync_ids.insert(id);
      }
    }
    object_invalidation_map =
        syncer::ObjectIdInvalidationMap::InvalidateAll(sync_ids);
  } else {
    invalidation::ObjectId object_id(
        object_source, ConvertJavaStringToUTF8(env, java_object_id));

    if (version == ipc::invalidation::Constants::UNKNOWN) {
      object_invalidation_map.Insert(
          syncer::Invalidation::InitUnknownVersion(object_id));
    } else {
      ObjectIdVersionMap::iterator it =
          max_invalidation_versions_.find(object_id);
      if ((it != max_invalidation_versions_.end()) && (version <= it->second)) {
        DVLOG(1) << "Dropping redundant invalidation with version " << version;
        return;
      }
      max_invalidation_versions_[object_id] = version;
      std::string payload;
      if (!java_payload.is_null())
        ConvertJavaStringToUTF8(env, java_payload, &payload);

      object_invalidation_map.Insert(
          syncer::Invalidation::Init(object_id, version, payload));
    }
  }

  invalidator_registrar_.DispatchInvalidationsToHandlers(
      object_invalidation_map);
  logger_.OnInvalidation(object_invalidation_map);
}

}  // namespace invalidation
