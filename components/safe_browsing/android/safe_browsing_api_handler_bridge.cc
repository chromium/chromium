// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/android/jni_headers/SafeBrowsingApiBridge_jni.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using content::BrowserThread;

namespace safe_browsing {

namespace {

void RunCallbackOnIOThread(
    std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback,
    SBThreatType threat_type,
    const ThreatMetadata& metadata) {
  CHECK(callback);              // Remove after fixing crbug.com/889972
  CHECK(!callback->is_null());  // Remove after fixing crbug.com/889972
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(std::move(*callback), threat_type, metadata));
}

void ReportUmaResult(safe_browsing::UmaRemoteCallResult result) {
  UMA_HISTOGRAM_ENUMERATION("SB2.RemoteCall.Result", result,
                            safe_browsing::UMA_STATUS_MAX_VALUE);
}

// Convert a SBThreatType to a Java threat type.  We only support a few.
int SBThreatTypeToJavaThreatType(const SBThreatType& sb_threat_type) {
  switch (sb_threat_type) {
    case SB_THREAT_TYPE_BILLING:
      return safe_browsing::JAVA_THREAT_TYPE_BILLING;
    case SB_THREAT_TYPE_SUBRESOURCE_FILTER:
      return safe_browsing::JAVA_THREAT_TYPE_SUBRESOURCE_FILTER;
    case SB_THREAT_TYPE_URL_PHISHING:
      return safe_browsing::JAVA_THREAT_TYPE_SOCIAL_ENGINEERING;
    case SB_THREAT_TYPE_URL_MALWARE:
      return safe_browsing::JAVA_THREAT_TYPE_POTENTIALLY_HARMFUL_APPLICATION;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return safe_browsing::JAVA_THREAT_TYPE_UNWANTED_SOFTWARE;
    case SB_THREAT_TYPE_CSD_WHITELIST:
      return safe_browsing::JAVA_THREAT_TYPE_CSD_ALLOWLIST;
    case SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST:
      return safe_browsing::JAVA_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST;
    default:
      NOTREACHED();
      return 0;
  }
}

// Convert a vector of SBThreatTypes to JavaIntArray of Java threat types.
ScopedJavaLocalRef<jintArray> SBThreatTypeSetToJavaArray(
    JNIEnv* env,
    const SBThreatTypeSet& threat_types) {
  DCHECK_LT(0u, threat_types.size());
  int int_threat_types[threat_types.size()];
  int* itr = &int_threat_types[0];
  for (auto threat_type : threat_types) {
    *itr++ = SBThreatTypeToJavaThreatType(threat_type);
  }
  return ToJavaIntArray(env, int_threat_types, threat_types.size());
}

// The map that holds the callback_id used to reference each pending request
// sent to Java, and the corresponding callback to call on receiving the
// response.
typedef std::unordered_map<
    jlong,
    std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta>>
    PendingCallbacksMap;

static PendingCallbacksMap* GetPendingCallbacksMapOnIOThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Holds the list of callback objects that we are currently waiting to hear
  // the result of from GmsCore.
  // The key is a unique count-up integer.
  static PendingCallbacksMap pending_callbacks;
  return &pending_callbacks;
}

}  // namespace

// Java->Native call, to check whether the feature to use local blacklists is
// enabled.
jboolean JNI_SafeBrowsingApiBridge_AreLocalBlacklistsEnabled(JNIEnv* env) {
  return base::FeatureList::IsEnabled(kUseLocalBlacklistsV2);
}

// Respond to the URL reputation request by looking up the callback information
// stored in |pending_callbacks|.
//   |callback_id| is an int form of pointer to a URLCheckCallbackMeta
//                 that will be called and then deleted here.
//   |result_status| is one of those from SafeBrowsingApiHandler.java
//   |metadata| is a JSON string classifying the threat if there is one.
void OnUrlCheckDoneOnIOThread(jlong callback_id,
                              jint result_status,
                              const std::string metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  PendingCallbacksMap* pending_callbacks = GetPendingCallbacksMapOnIOThread();
  bool found = base::Contains(*pending_callbacks, callback_id);
  DCHECK(found) << "Not found in pending_callbacks: " << callback_id;
  if (!found)
    return;

  std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback =
      std::move((*pending_callbacks)[callback_id]);
  CHECK(callback);  // Remove after fixing crbug.com/889972
  pending_callbacks->erase(callback_id);

  if (result_status != RESULT_STATUS_SUCCESS) {
    if (result_status == RESULT_STATUS_TIMEOUT) {
      CHECK(!callback->is_null());  // Remove after fixing crbug.com/889972

      ReportUmaResult(UMA_STATUS_TIMEOUT);
    } else {
      CHECK(!callback->is_null());  // Remove after fixing crbug.com/889972

      DCHECK_EQ(result_status, RESULT_STATUS_INTERNAL_ERROR);
      ReportUmaResult(UMA_STATUS_INTERNAL_ERROR);
    }
    std::move(*callback).Run(SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  // Shortcut for safe, so we don't have to parse JSON.
  if (metadata == "{}") {
    CHECK(!callback->is_null());  // Remove after fixing crbug.com/889972

    ReportUmaResult(UMA_STATUS_SAFE);
    std::move(*callback).Run(SB_THREAT_TYPE_SAFE, ThreatMetadata());
  } else {
    CHECK(!callback->is_null());  // Remove after fixing crbug.com/889972

    // Unsafe, assuming we can parse the JSON.
    SBThreatType worst_threat;
    ThreatMetadata threat_metadata;
    ReportUmaResult(
        ParseJsonFromGMSCore(metadata, &worst_threat, &threat_metadata));

    std::move(*callback).Run(worst_threat, threat_metadata);
  }
}

// Java->Native call, invoked when a check is done.
//   |callback_id| is a key into the |pending_callbacks_| map, whose value is a
//                 URLCheckCallbackMeta that will be called and then deleted on
//                 the IO thread.
//   |result_status| is one of those from SafeBrowsingApiHandler.java
//   |metadata| is a JSON string classifying the threat if there is one.
//   |check_delta| is the number of microseconds it took to look up the URL
//                 reputation from GmsCore.
//
//   Careful note: this can be called on multiple threads, so make sure there is
//   nothing thread unsafe happening here.
void JNI_SafeBrowsingApiBridge_OnUrlCheckDone(
    JNIEnv* env,
    jlong callback_id,
    jint result_status,
    const JavaParamRef<jstring>& metadata,
    jlong check_delta) {
  UMA_HISTOGRAM_COUNTS_10M("SB2.RemoteCall.CheckDelta", check_delta);

  const std::string metadata_str =
      (metadata ? ConvertJavaStringToUTF8(env, metadata) : "");

  TRACE_EVENT1("safe_browsing", "SafeBrowsingApiHandlerBridge::OnUrlCheckDone",
               "metadata", metadata_str);

  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(&OnUrlCheckDoneOnIOThread, callback_id,
                                result_status, metadata_str));
}

//
// SafeBrowsingApiHandlerBridge
//
SafeBrowsingApiHandlerBridge::SafeBrowsingApiHandlerBridge()
    : checked_api_support_(false) {}

SafeBrowsingApiHandlerBridge::~SafeBrowsingApiHandlerBridge() {}

bool SafeBrowsingApiHandlerBridge::CheckApiIsSupported() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!checked_api_support_) {
    j_api_handler_ = base::android::ScopedJavaGlobalRef<jobject>(
        Java_SafeBrowsingApiBridge_create(AttachCurrentThread()));
    checked_api_support_ = true;
  }
  return j_api_handler_.obj() != nullptr;
}

bool SafeBrowsingApiHandlerBridge::StartAllowlistCheck(
    const GURL& url,
    const SBThreatType& sb_threat_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!CheckApiIsSupported()) {
    return false;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  int j_threat_type = SBThreatTypeToJavaThreatType(sb_threat_type);
  return Java_SafeBrowsingApiBridge_startAllowlistLookup(env, j_api_handler_,
                                                         j_url, j_threat_type);
}

std::string SafeBrowsingApiHandlerBridge::GetSafetyNetId() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  bool feature_enabled = base::FeatureList::IsEnabled(kCaptureSafetyNetId);
  DCHECK(feature_enabled);

  static std::string safety_net_id;
  if (feature_enabled && CheckApiIsSupported() && safety_net_id.empty()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> jsafety_net_id =
        Java_SafeBrowsingApiBridge_getSafetyNetId(env, j_api_handler_);
    safety_net_id =
        jsafety_net_id ? ConvertJavaStringToUTF8(env, jsafety_net_id) : "";
  }

  return safety_net_id;
}

void SafeBrowsingApiHandlerBridge::StartURLCheck(
    std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!CheckApiIsSupported()) {
    // Mark all requests as safe. Only users who have an old, broken GMSCore or
    // have sideloaded Chrome w/o PlayStore should land here.
    RunCallbackOnIOThread(std::move(callback), SB_THREAT_TYPE_SAFE,
                          ThreatMetadata());
    ReportUmaResult(UMA_STATUS_UNSUPPORTED);
    return;
  }

  jlong callback_id = next_callback_id_++;
  GetPendingCallbacksMapOnIOThread()->insert(
      {callback_id, std::move(callback)});

  DCHECK(!threat_types.empty());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jintArray> j_threat_types =
      SBThreatTypeSetToJavaArray(env, threat_types);

  Java_SafeBrowsingApiBridge_startUriLookup(env, j_api_handler_, callback_id,
                                            j_url, j_threat_types);
}

bool SafeBrowsingApiHandlerBridge::StartCSDAllowlistCheck(const GURL& url) {
  return StartAllowlistCheck(url, safe_browsing::SB_THREAT_TYPE_CSD_WHITELIST);
}

bool SafeBrowsingApiHandlerBridge::StartHighConfidenceAllowlistCheck(
    const GURL& url) {
  return StartAllowlistCheck(
      url, safe_browsing::SB_THREAT_TYPE_HIGH_CONFIDENCE_ALLOWLIST);
}

}  // namespace safe_browsing
