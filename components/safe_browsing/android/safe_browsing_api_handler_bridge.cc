// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "jni/SafeBrowsingApiBridge_jni.h"

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
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
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

}  // namespace

// Java->Native call, to check whether the feature to use local blacklists is
// enabled.
jboolean JNI_SafeBrowsingApiBridge_AreLocalBlacklistsEnabled(
    JNIEnv* env,
    const JavaParamRef<jclass>&) {
  return base::FeatureList::IsEnabled(kUseLocalBlacklistsV2);
}

// Java->Native call, invoked when a check is done.
//   |callback_id| is an int form of pointer to a URLCheckCallbackMeta
//                 that will be called and then deleted here.
//   |result_status| is one of those from SafeBrowsingApiHandler.java
//   |metadata| is a JSON string classifying the threat if there is one.
//
//   Careful note: this can be called on multiple threads, so make sure there is
//   nothing thread unsafe happening here.
void JNI_SafeBrowsingApiBridge_OnUrlCheckDone(
    JNIEnv* env,
    const JavaParamRef<jclass>& context,
    jlong callback_id,
    jint result_status,
    const JavaParamRef<jstring>& metadata,
    jlong check_delta) {
  DCHECK(callback_id);
  UMA_HISTOGRAM_COUNTS_10M("SB2.RemoteCall.CheckDelta", check_delta);

  const std::string metadata_str =
      (metadata ? ConvertJavaStringToUTF8(env, metadata) : "");

  TRACE_EVENT1("safe_browsing", "SafeBrowsingApiHandlerBridge::OnUrlCheckDone",
               "metadata", metadata_str);

  DVLOG(1) << "OnURLCheckDone invoked for check " << callback_id
           << " with status=" << result_status << " and metadata=["
           << metadata_str << "]";

  // Convert java long long int to c++ pointer, take ownership.
  std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback(
      reinterpret_cast<SafeBrowsingApiHandlerBridge::URLCheckCallbackMeta*>(
          callback_id));

  if (result_status != RESULT_STATUS_SUCCESS) {
    if (result_status == RESULT_STATUS_TIMEOUT) {
      ReportUmaResult(UMA_STATUS_TIMEOUT);
      VLOG(1) << "Safe browsing API call timed-out";
    } else {
      DCHECK_EQ(result_status, RESULT_STATUS_INTERNAL_ERROR);
      ReportUmaResult(UMA_STATUS_INTERNAL_ERROR);
    }
    RunCallbackOnIOThread(std::move(callback), SB_THREAT_TYPE_SAFE,
                          ThreatMetadata());
    return;
  }

  // Shortcut for safe, so we don't have to parse JSON.
  if (metadata_str == "{}") {
    ReportUmaResult(UMA_STATUS_SAFE);
    RunCallbackOnIOThread(std::move(callback), SB_THREAT_TYPE_SAFE,
                          ThreatMetadata());
  } else {
    // Unsafe, assuming we can parse the JSON.
    SBThreatType worst_threat;
    ThreatMetadata threat_metadata;
    ReportUmaResult(
        ParseJsonFromGMSCore(metadata_str, &worst_threat, &threat_metadata));
    if (worst_threat != SB_THREAT_TYPE_SAFE) {
      DVLOG(1) << "Check " << callback_id << " was a MATCH";
    }

    RunCallbackOnIOThread(std::move(callback), worst_threat, threat_metadata);
  }
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
    DVLOG(1) << "Checking API support.";
    j_api_handler_ = base::android::ScopedJavaGlobalRef<jobject>(
        Java_SafeBrowsingApiBridge_create(AttachCurrentThread()));
    checked_api_support_ = true;
  }
  return j_api_handler_.obj() != nullptr;
}

void SafeBrowsingApiHandlerBridge::StartURLCheck(
    std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  if (!CheckApiIsSupported()) {
    // Mark all requests as safe. Only users who have an old, broken GMSCore or
    // have sideloaded Chrome w/o PlayStore should land here.
    RunCallbackOnIOThread(std::move(callback), SB_THREAT_TYPE_SAFE,
                          ThreatMetadata());
    ReportUmaResult(UMA_STATUS_UNSUPPORTED);
    return;
  }

  // Save the address on the heap so we can pass it through JNI. The unique ptr
  // releases ownership, we will re-own this callback when the response is
  // received in JNI_SafeBrowsingApiBridge_OnUrlCheckDone.
  intptr_t callback_id = reinterpret_cast<intptr_t>(callback.release());
  DVLOG(1) << "Starting check " << callback_id << " for URL " << url;

  DCHECK(!threat_types.empty());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jintArray> j_threat_types =
      SBThreatTypeSetToJavaArray(env, threat_types);

  base::ElapsedTimer check_timer;
  Java_SafeBrowsingApiBridge_startUriLookup(env, j_api_handler_, callback_id,
                                            j_url, j_threat_types);
  // TODO(vakh): The following metric isn't very useful now since the
  // |startUriLookup| method simply posts a task and adds listeners now.
  // Continue to monitor it to ensure that it keeps falling and then remove it
  // when it is consistently a low value. (https://crbug.com/839190)
  UMA_HISTOGRAM_COUNTS_10M("SB2.RemoteCall.CheckDispatchTime",
                           check_timer.Elapsed().InMicroseconds());
}

}  // namespace safe_browsing
