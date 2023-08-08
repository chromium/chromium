// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/android/jni_headers/SafeBrowsingApiBridge_jni.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using content::BrowserThread;

namespace safe_browsing {

namespace {

void RunCallbackOnSBThread(
    std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  auto task_runner = base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                         ? content::GetUIThreadTaskRunner({})
                         : content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(*callback), threat_type, metadata));
}

void ReportUmaResult(UmaRemoteCallResult result) {
  UMA_HISTOGRAM_ENUMERATION("SB2.RemoteCall.Result", result,
                            UmaRemoteCallResult::MAX_VALUE);
}

// Convert a SBThreatType to a Java SafetyNet API threat type.  We only support
// a few.
SafetyNetJavaThreatType SBThreatTypeToSafetyNetJavaThreatType(
    const SBThreatType& sb_threat_type) {
  switch (sb_threat_type) {
    case SB_THREAT_TYPE_BILLING:
      return SafetyNetJavaThreatType::BILLING;
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      return SafetyNetJavaThreatType::SUBRESOURCE_FILTER;
    case SB_THREAT_TYPE_URL_PHISHING:
      return SafetyNetJavaThreatType::SOCIAL_ENGINEERING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return SafetyNetJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return SafetyNetJavaThreatType::UNWANTED_SOFTWARE;
    case SB_THREAT_TYPE_CSD_ALLOWLIST:
      return SafetyNetJavaThreatType::CSD_ALLOWLIST;
    default:
      NOTREACHED();
      return SafetyNetJavaThreatType::MAX_VALUE;
  }
}

// Convert a vector of SBThreatTypes to JavaIntArray of Java SafetyNet API
// threat types.
ScopedJavaLocalRef<jintArray> SBThreatTypeSetToSafetyNetJavaArray(
    JNIEnv* env,
    const SBThreatTypeSet& threat_types) {
  DCHECK_LT(0u, threat_types.size());
  int int_threat_types[threat_types.size()];
  int* itr = &int_threat_types[0];
  for (auto threat_type : threat_types) {
    *itr++ =
        static_cast<int>(SBThreatTypeToSafetyNetJavaThreatType(threat_type));
  }
  return ToJavaIntArray(env, int_threat_types, threat_types.size());
}

// Convert a Java threat type for SafeBrowsing to a SBThreatType.
SBThreatType SafeBrowsingJavaToSBThreatType(
    SafeBrowsingJavaThreatType java_threat_num) {
  switch (java_threat_num) {
    case SafeBrowsingJavaThreatType::NO_THREAT:
      return SB_THREAT_TYPE_SAFE;
    case SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case SafeBrowsingJavaThreatType::SUBRESOURCE_FILTER:
      return SB_THREAT_TYPE_SUBRESOURCE_FILTER;
    case SafeBrowsingJavaThreatType::BILLING:
      return SB_THREAT_TYPE_BILLING;
  }
}

// Convert a SBThreatType to a Java threat type for SafeBrowsing. We only
// support a few.
SafeBrowsingJavaThreatType SBThreatTypeToSafeBrowsingApiJavaThreatType(
    const SBThreatType& sb_threat_type) {
  switch (sb_threat_type) {
    case SB_THREAT_TYPE_URL_UNWANTED:
      return SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE;
    case SB_THREAT_TYPE_URL_MALWARE:
      return SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION;
    case SB_THREAT_TYPE_URL_PHISHING:
      return SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING;
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      return SafeBrowsingJavaThreatType::SUBRESOURCE_FILTER;
    case SB_THREAT_TYPE_BILLING:
      return SafeBrowsingJavaThreatType::BILLING;
    default:
      NOTREACHED();
      return SafeBrowsingJavaThreatType::NO_THREAT;
  }
}

// Convert a vector of SBThreatTypes to JavaIntArray of SafeBrowsing API's
// threat types.
ScopedJavaLocalRef<jintArray> SBThreatTypeSetToSafeBrowsingJavaArray(
    JNIEnv* env,
    const SBThreatTypeSet& threat_types) {
  DCHECK_LT(0u, threat_types.size());
  int int_threat_types[threat_types.size()];
  int* itr = &int_threat_types[0];
  for (auto threat_type : threat_types) {
    *itr++ = static_cast<int>(
        SBThreatTypeToSafeBrowsingApiJavaThreatType(threat_type));
  }
  return ToJavaIntArray(env, int_threat_types, threat_types.size());
}

// The map that holds the callback_id used to reference each pending request
// sent to Java, and the corresponding callback to call on receiving the
// response.
using PendingCallbacksMap = std::unordered_map<
    jlong,
    std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback>>;

PendingCallbacksMap& GetPendingSafetyNetCallbacksMapOnSBThread() {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);

  // Holds the list of callback objects that we are currently waiting to hear
  // the result of from GmsCore.
  // The key is a unique count-up integer.
  static base::NoDestructor<PendingCallbacksMap> pending_safety_net_callbacks;
  return *pending_safety_net_callbacks;
}

PendingCallbacksMap& GetPendingSafeBrowsingCallbacksMapOnSBThread() {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);

  // Holds the list of callback objects that we are currently waiting to hear
  // the result of from GmsCore.
  // The key is a unique count-up integer.
  static base::NoDestructor<PendingCallbacksMap>
      pending_safe_browsing_callbacks;
  return *pending_safe_browsing_callbacks;
}

bool StartAllowlistCheck(const GURL& url, const SBThreatType& sb_threat_type) {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);
  JNIEnv* env = AttachCurrentThread();
  if (!Java_SafeBrowsingApiBridge_ensureSafetyNetApiInitialized(env)) {
    return false;
  }

  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  int j_threat_type =
      static_cast<int>(SBThreatTypeToSafetyNetJavaThreatType(sb_threat_type));
  return Java_SafeBrowsingApiBridge_startAllowlistLookup(env, j_url,
                                                         j_threat_type);
}

}  // namespace

// static
SafeBrowsingApiHandlerBridge& SafeBrowsingApiHandlerBridge::GetInstance() {
  static base::NoDestructor<SafeBrowsingApiHandlerBridge> instance;
  return *instance.get();
}

// Respond to the URL reputation request by looking up the callback information
// stored in |pending_safety_net_callbacks|.
//   |callback_id| is an int form of pointer to a ::ResponseCallback
//                 that will be called and then deleted here.
//   |j_result_status| is one of those from SafeBrowsingApiHandlerBridge.java
//   |metadata| is a JSON string classifying the threat if there is one.
void OnUrlCheckDoneOnSBThreadBySafetyNetApi(jlong callback_id,
                                            jint j_result_status,
                                            const std::string metadata) {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);

  PendingCallbacksMap& pending_callbacks =
      GetPendingSafetyNetCallbacksMapOnSBThread();
  bool found = base::Contains(pending_callbacks, callback_id);
  DCHECK(found) << "Not found in pending_safety_net_callbacks: " << callback_id;
  if (!found)
    return;

  std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback =
      std::move((pending_callbacks)[callback_id]);
  pending_callbacks.erase(callback_id);

  SafetyNetRemoteCallResultStatus result_status =
      static_cast<SafetyNetRemoteCallResultStatus>(j_result_status);
  if (result_status != SafetyNetRemoteCallResultStatus::SUCCESS) {
    if (result_status == SafetyNetRemoteCallResultStatus::TIMEOUT) {
      ReportUmaResult(UmaRemoteCallResult::TIMEOUT);
    } else {
      DCHECK_EQ(result_status, SafetyNetRemoteCallResultStatus::INTERNAL_ERROR);
      ReportUmaResult(UmaRemoteCallResult::INTERNAL_ERROR);
    }
    std::move(*callback).Run(SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  // Shortcut for safe, so we don't have to parse JSON.
  if (metadata == "{}") {
    ReportUmaResult(UmaRemoteCallResult::SAFE);
    std::move(*callback).Run(SB_THREAT_TYPE_SAFE, ThreatMetadata());
  } else {
    // Unsafe, assuming we can parse the JSON.
    SBThreatType worst_threat;
    ThreatMetadata threat_metadata;
    ReportUmaResult(
        ParseJsonFromGMSCore(metadata, &worst_threat, &threat_metadata));

    std::move(*callback).Run(worst_threat, threat_metadata);
  }
}

// Java->Native call, invoked when a SafetyNet check is done.
//   |callback_id| is a key into the |pending_safety_net_callbacks| map, whose
//   value is a ::ResponseCallback that will be called and then deleted on
//   the IO thread.
//   |result_status| is a @SafeBrowsingResult from SafetyNetApiHandler.java
//   |metadata| is a JSON string classifying the threat if there is one.
//   |check_delta| is the number of microseconds it took to look up the URL
//                 reputation from GmsCore.
//
//   Careful note: this can be called on multiple threads, so make sure there is
//   nothing thread unsafe happening here.
void JNI_SafeBrowsingApiBridge_OnUrlCheckDoneBySafetyNetApi(
    JNIEnv* env,
    jlong callback_id,
    jint result_status,
    const JavaParamRef<jstring>& metadata,
    jlong check_delta) {
  UMA_HISTOGRAM_COUNTS_10M("SB2.RemoteCall.CheckDelta", check_delta);

  const std::string metadata_str =
      (metadata ? ConvertJavaStringToUTF8(env, metadata) : "");

  TRACE_EVENT1("safe_browsing",
               "SafeBrowsingApiHandlerBridge::nUrlCheckDoneBySafetyNetApi",
               "metadata", metadata_str);

  auto task_runner =
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::GetUIThreadTaskRunner({})
          : content::GetIOThreadTaskRunner({});
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&OnUrlCheckDoneOnSBThreadBySafetyNetApi,
                                callback_id, result_status, metadata_str));
}

// Respond to the URL reputation request by looking up the callback information
// stored in |pending_safe_browsing_callbacks|. Must be called on the original
// thread that starts the lookup.
void OnUrlCheckDoneOnSBThreadBySafeBrowsingApi(
    jlong callback_id,
    SafeBrowsingApiLookupResult lookup_result,
    SafeBrowsingJavaThreatType threat_type,
    std::vector<int> threat_attributes,
    SafeBrowsingJavaResponseStatus response_status) {
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);

  PendingCallbacksMap& pending_callbacks =
      GetPendingSafeBrowsingCallbacksMapOnSBThread();
  bool found = base::Contains(pending_callbacks, callback_id);
  DCHECK(found) << "Not found in pending_safe_browsing_callbacks: "
                << callback_id;
  if (!found) {
    return;
  }

  std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback =
      std::move((pending_callbacks)[callback_id]);
  pending_callbacks.erase(callback_id);

  // TODO(crbug.com/1444511): Consume other fields before returning the
  // threat_type.
  std::move(*callback).Run(SafeBrowsingJavaToSBThreatType(threat_type),
                           ThreatMetadata());
}

// Java->Native call, invoked when a SafeBrowsing check is done. |env| is the
// JNI environment that stores local pointers. |callback_id| is a key into the
// |pending_safe_browsing_callbacks| map, whose value is a ::ResponseCallback
// that will be called and then deleted on the IO thread. |j_lookup_result| is a
// @LookupResult from SafeBrowsingApiHandler.java. |j_threat_type| is the threat
// type that matched against the URL. |j_threat_attributes| is the threat
// attributes that matched against the URL. |j_response_status| reflects how the
// API gets the response. |check_delta_ms| is the number of microseconds it
// took to look up the URL reputation from GmsCore.
//
// Careful note: this can be called on multiple threads, so make sure there is
// nothing thread unsafe happening here.
void JNI_SafeBrowsingApiBridge_OnUrlCheckDoneBySafeBrowsingApi(
    JNIEnv* env,
    jlong callback_id,
    jint j_lookup_result,
    jint j_threat_type,
    const JavaParamRef<jintArray>& j_threat_attributes,
    jint j_response_status,
    jlong check_delta_ms) {
  // TODO(crbug.com/1444511): Add a histogram to log check_delta_ms.
  auto task_runner =
      base::FeatureList::IsEnabled(safe_browsing::kSafeBrowsingOnUIThread)
          ? content::GetUIThreadTaskRunner({})
          : content::GetIOThreadTaskRunner({});

  // TODO(crbug.com/1444511): Add a check that j_threat_type,
  // j_threat_attributes and j_response_status are all defined values (in case
  // that there is a mismatch between Clank and SafeBrowsing API).
  SafeBrowsingApiLookupResult lookup_result =
      static_cast<SafeBrowsingApiLookupResult>(j_lookup_result);
  SafeBrowsingJavaThreatType threat_type =
      static_cast<SafeBrowsingJavaThreatType>(j_threat_type);
  std::vector<int> threat_attributes;
  JavaIntArrayToIntVector(env, j_threat_attributes, &threat_attributes);
  SafeBrowsingJavaResponseStatus response_status =
      static_cast<SafeBrowsingJavaResponseStatus>(j_response_status);
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&OnUrlCheckDoneOnSBThreadBySafeBrowsingApi,
                                callback_id, lookup_result, threat_type,
                                std::move(threat_attributes), response_status));
}

//
// SafeBrowsingApiHandlerBridge
//
SafeBrowsingApiHandlerBridge::~SafeBrowsingApiHandlerBridge() {}

void SafeBrowsingApiHandlerBridge::StartHashDatabaseUrlCheck(
    std::unique_ptr<ResponseCallback> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  StartUrlCheckBySafetyNet(std::move(callback), url, threat_types);
}

void SafeBrowsingApiHandlerBridge::StartHashRealTimeUrlCheck(
    std::unique_ptr<ResponseCallback> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  StartUrlCheckBySafeBrowsing(std::move(callback), url, threat_types,
                              SafeBrowsingJavaProtocol::REAL_TIME);
}

void SafeBrowsingApiHandlerBridge::StartUrlCheckBySafetyNet(
    std::unique_ptr<ResponseCallback> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  if (interceptor_for_testing_) {
    // For testing, only check the interceptor.
    interceptor_for_testing_->CheckBySafetyNet(std::move(callback), url);
    return;
  }
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);
  JNIEnv* env = AttachCurrentThread();
  if (!Java_SafeBrowsingApiBridge_ensureSafetyNetApiInitialized(env)) {
    // Mark all requests as safe. Only users who have an old, broken GMSCore or
    // have sideloaded Chrome w/o PlayStore should land here.
    RunCallbackOnSBThread(std::move(callback), SB_THREAT_TYPE_SAFE,
                          ThreatMetadata());
    ReportUmaResult(UmaRemoteCallResult::UNSUPPORTED);
    return;
  }

  jlong callback_id = next_safety_net_callback_id_++;
  GetPendingSafetyNetCallbacksMapOnSBThread().insert(
      {callback_id, std::move(callback)});

  DCHECK(!threat_types.empty());

  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jintArray> j_threat_types =
      SBThreatTypeSetToSafetyNetJavaArray(env, threat_types);

  Java_SafeBrowsingApiBridge_startUriLookupBySafetyNetApi(
      env, callback_id, j_url, j_threat_types);
}

void SafeBrowsingApiHandlerBridge::StartUrlCheckBySafeBrowsing(
    std::unique_ptr<ResponseCallback> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    const SafeBrowsingJavaProtocol& protocol) {
  if (interceptor_for_testing_) {
    // For testing, only check the interceptor.
    interceptor_for_testing_->CheckBySafeBrowsing(std::move(callback), url);
    return;
  }
  DCHECK_CURRENTLY_ON(base::FeatureList::IsEnabled(kSafeBrowsingOnUIThread)
                          ? content::BrowserThread::UI
                          : content::BrowserThread::IO);
  JNIEnv* env = AttachCurrentThread();

  // TODO(crbug.com/1444511): Check if the device has required GMSCore version.
  // If not, fall back to hash database check through SafetyNet API. Also add a
  // histogram to track the proportion of users who don't have required version
  // to inform when we can remove the fallback.

  jlong callback_id = next_safe_browsing_callback_id_++;
  GetPendingSafeBrowsingCallbacksMapOnSBThread().insert(
      {callback_id, std::move(callback)});

  DCHECK(!threat_types.empty());

  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jintArray> j_threat_types =
      SBThreatTypeSetToSafeBrowsingJavaArray(env, threat_types);
  int j_int_protocol = static_cast<int>(protocol);

  Java_SafeBrowsingApiBridge_startUriLookupBySafeBrowsingApi(
      env, callback_id, j_url, j_threat_types, j_int_protocol);
}

bool SafeBrowsingApiHandlerBridge::StartCSDAllowlistCheck(const GURL& url) {
  if (interceptor_for_testing_)
    return false;
  return StartAllowlistCheck(url, safe_browsing::SB_THREAT_TYPE_CSD_ALLOWLIST);
}

}  // namespace safe_browsing
