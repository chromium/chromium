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

void SafeBrowsingApiHandlerBridge::StartUrlCheckBySafetyNet(
    std::unique_ptr<ResponseCallback> callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  if (interceptor_for_testing_) {
    // For testing, only check the interceptor.
    interceptor_for_testing_->Check(std::move(callback), url);
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

bool SafeBrowsingApiHandlerBridge::StartCSDAllowlistCheck(const GURL& url) {
  if (interceptor_for_testing_)
    return false;
  return StartAllowlistCheck(url, safe_browsing::SB_THREAT_TYPE_CSD_ALLOWLIST);
}

}  // namespace safe_browsing
