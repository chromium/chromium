// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"

#include <memory>
#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/containers/heap_array.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_tokenizer.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/fixed_array.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/safebrowsing_switches.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/safe_browsing/android/jni_headers/SafeBrowsingApiBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaIntArrayToIntVector;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;
using content::BrowserThread;

namespace safe_browsing {

namespace {

std::string GetSafeBrowsingJavaProtocolUmaSuffix(
    SafeBrowsingJavaProtocol protocol) {
  switch (protocol) {
    case SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST:
      return ".LocalBlocklist";
    case SafeBrowsingJavaProtocol::REAL_TIME:
      return ".RealTime";
  }
}

void ReportSafeBrowsingJavaValidationResult(
    SafeBrowsingJavaProtocol protocol,
    SafeBrowsingJavaValidationResult validation_result) {
  base::UmaHistogramEnumeration(
      "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult",
      validation_result);
  base::UmaHistogramEnumeration(
      "SafeBrowsing.GmsSafeBrowsingApi.JavaValidationResult" +
          GetSafeBrowsingJavaProtocolUmaSuffix(protocol),
      validation_result);
}

void ReportUmaHistogramSparseWithAndWithoutSuffix(const std::string& metric,
                                                  const std::string& suffix,
                                                  int value) {
  base::UmaHistogramSparse(metric, value);
  base::UmaHistogramSparse(metric + suffix, value);
}

void ReportSafeBrowsingJavaResponse(
    SafeBrowsingJavaProtocol protocol,
    SafeBrowsingApiLookupResult lookup_result,
    SafeBrowsingJavaThreatType threat_type,
    const std::vector<int>& threat_attributes,
    SafeBrowsingJavaResponseStatus response_status,
    jlong check_delta_microseconds) {
  std::string suffix = GetSafeBrowsingJavaProtocolUmaSuffix(protocol);

  base::UmaHistogramMicrosecondsTimes(
      "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta",
      base::Microseconds(check_delta_microseconds));
  base::UmaHistogramMicrosecondsTimes(
      "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta" + suffix,
      base::Microseconds(check_delta_microseconds));

  ReportUmaHistogramSparseWithAndWithoutSuffix(
      "SafeBrowsing.GmsSafeBrowsingApi.LookupResult", suffix,
      static_cast<int>(lookup_result));
  if (lookup_result != SafeBrowsingApiLookupResult::SUCCESS) {
    // Do not log other histograms if the lookup failed, since the other values
    // will all be dummy values.
    return;
  }
  ReportUmaHistogramSparseWithAndWithoutSuffix(
      "SafeBrowsing.GmsSafeBrowsingApi.ThreatType2", suffix,
      static_cast<int>(threat_type));
  base::UmaHistogramCounts100(
      "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount",
      threat_attributes.size());
  base::UmaHistogramCounts100(
      "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttributeCount" + suffix,
      threat_attributes.size());
  for (int threat_attribute : threat_attributes) {
    ReportUmaHistogramSparseWithAndWithoutSuffix(
        "SafeBrowsing.GmsSafeBrowsingApi.ThreatAttribute", suffix,
        threat_attribute);
  }
  ReportUmaHistogramSparseWithAndWithoutSuffix(
      "SafeBrowsing.GmsSafeBrowsingApi.ResponseStatus", suffix,
      static_cast<int>(response_status));

  if (response_status ==
      SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME) {
    base::UmaHistogramMicrosecondsTimes(
        "SafeBrowsing.GmsSafeBrowsingApi.CheckDelta.SuccessWithRealTime",
        base::Microseconds(check_delta_microseconds));
  }
}

SafeBrowsingJavaValidationResult GetJavaValidationResult(
    SafeBrowsingApiLookupResult lookup_result,
    SafeBrowsingJavaThreatType threat_type,
    const std::vector<int>& threat_attributes,
    SafeBrowsingJavaResponseStatus response_status) {
  bool is_lookup_result_recognized = false;
  switch (lookup_result) {
    case SafeBrowsingApiLookupResult::SUCCESS:
    case SafeBrowsingApiLookupResult::FAILURE:
    case SafeBrowsingApiLookupResult::FAILURE_API_CALL_TIMEOUT:
    case SafeBrowsingApiLookupResult::FAILURE_API_UNSUPPORTED:
    case SafeBrowsingApiLookupResult::FAILURE_API_NOT_AVAILABLE:
    case SafeBrowsingApiLookupResult::FAILURE_HANDLER_NULL:
      is_lookup_result_recognized = true;
      break;
  }
  if (!is_lookup_result_recognized) {
    return SafeBrowsingJavaValidationResult::INVALID_LOOKUP_RESULT;
  }

  bool is_threat_type_recognized = false;
  switch (threat_type) {
    case SafeBrowsingJavaThreatType::NO_THREAT:
    case SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING:
    case SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE:
    case SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
    case SafeBrowsingJavaThreatType::BILLING:
    case SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
    case SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION:
      is_threat_type_recognized = true;
      break;
  }
  if (!is_threat_type_recognized) {
    return SafeBrowsingJavaValidationResult::INVALID_THREAT_TYPE;
  }

  for (int threat_attribute : threat_attributes) {
    SafeBrowsingJavaThreatAttribute threat_attribute_enum =
        static_cast<SafeBrowsingJavaThreatAttribute>(threat_attribute);
    bool is_threat_attribute_recognized = false;
    switch (threat_attribute_enum) {
      case SafeBrowsingJavaThreatAttribute::CANARY:
      case SafeBrowsingJavaThreatAttribute::FRAME_ONLY:
        is_threat_attribute_recognized = true;
        break;
    }
    if (!is_threat_attribute_recognized) {
      return SafeBrowsingJavaValidationResult::INVALID_THREAT_ATTRIBUTE;
    }
  }

  bool is_reponse_status_recognized = false;
  switch (response_status) {
    case SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST:
    case SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME:
    case SafeBrowsingJavaResponseStatus::SUCCESS_FALLBACK_REAL_TIME_TIMEOUT:
    case SafeBrowsingJavaResponseStatus::SUCCESS_FALLBACK_REAL_TIME_THROTTLED:
    case SafeBrowsingJavaResponseStatus::FAILURE_NETWORK_UNAVAILABLE:
    case SafeBrowsingJavaResponseStatus::FAILURE_BLOCK_LIST_UNAVAILABLE:
    case SafeBrowsingJavaResponseStatus::FAILURE_INVALID_URL:
      is_reponse_status_recognized = true;
      break;
  }
  if (!is_reponse_status_recognized) {
    return SafeBrowsingJavaValidationResult::
        VALID_WITH_UNRECOGNIZED_RESPONSE_STATUS;
  }

  return SafeBrowsingJavaValidationResult::VALID;
}

// Validate the values returned from SafeBrowsing API are defined in enum. The
// response can be out of range if there is version mismatch between Chrome and
// the GMSCore APK, or the enums between c++ and java are not aligned.
bool IsResponseFromJavaValid(SafeBrowsingJavaProtocol protocol,
                             SafeBrowsingApiLookupResult lookup_result,
                             SafeBrowsingJavaThreatType threat_type,
                             const std::vector<int>& threat_attributes,
                             SafeBrowsingJavaResponseStatus response_status) {
  SafeBrowsingJavaValidationResult validation_result = GetJavaValidationResult(
      lookup_result, threat_type, threat_attributes, response_status);
  ReportSafeBrowsingJavaValidationResult(protocol, validation_result);

  switch (validation_result) {
    case SafeBrowsingJavaValidationResult::VALID:
    // Not returning false if response_status is unrecognized. This is to avoid
    // the API adding a new success response_status while we haven't integrated
    // the new value yet. In this case, we still want to return the threat_type.
    case SafeBrowsingJavaValidationResult::
        VALID_WITH_UNRECOGNIZED_RESPONSE_STATUS:
      return true;
    case SafeBrowsingJavaValidationResult::INVALID_LOOKUP_RESULT:
    case SafeBrowsingJavaValidationResult::INVALID_THREAT_TYPE:
    case SafeBrowsingJavaValidationResult::INVALID_THREAT_ATTRIBUTE:
      return false;
  }
}

bool IsLookupSuccessful(SafeBrowsingApiLookupResult lookup_result,
                        SafeBrowsingJavaResponseStatus response_status) {
  bool is_lookup_result_success = false;
  switch (lookup_result) {
    case SafeBrowsingApiLookupResult::SUCCESS:
      is_lookup_result_success = true;
      break;
    case SafeBrowsingApiLookupResult::FAILURE:
    case SafeBrowsingApiLookupResult::FAILURE_API_CALL_TIMEOUT:
    case SafeBrowsingApiLookupResult::FAILURE_API_UNSUPPORTED:
    case SafeBrowsingApiLookupResult::FAILURE_API_NOT_AVAILABLE:
    case SafeBrowsingApiLookupResult::FAILURE_HANDLER_NULL:
      break;
  }
  if (!is_lookup_result_success) {
    return false;
  }

  // Note that we check explicit failure statuses instead of success statuses.
  // This is to avoid the API adding a new success response_status while we
  // haven't integrated the new value yet. The impact of a missing failure
  // status is smaller since the API is expected to return a safe threat type in
  // a failure anyway.
  bool is_response_status_success = true;
  switch (response_status) {
    case SafeBrowsingJavaResponseStatus::SUCCESS_WITH_LOCAL_BLOCKLIST:
    case SafeBrowsingJavaResponseStatus::SUCCESS_WITH_REAL_TIME:
    case SafeBrowsingJavaResponseStatus::SUCCESS_FALLBACK_REAL_TIME_TIMEOUT:
    case SafeBrowsingJavaResponseStatus::SUCCESS_FALLBACK_REAL_TIME_THROTTLED:
      break;
    case SafeBrowsingJavaResponseStatus::FAILURE_NETWORK_UNAVAILABLE:
    case SafeBrowsingJavaResponseStatus::FAILURE_BLOCK_LIST_UNAVAILABLE:
    case SafeBrowsingJavaResponseStatus::FAILURE_INVALID_URL:
      is_response_status_success = false;
      break;
  }
  return is_response_status_success;
}

bool IsSafeBrowsingNonRecoverable(SafeBrowsingApiLookupResult lookup_result) {
  switch (lookup_result) {
    case SafeBrowsingApiLookupResult::FAILURE_API_UNSUPPORTED:
    case SafeBrowsingApiLookupResult::FAILURE_API_NOT_AVAILABLE:
    case SafeBrowsingApiLookupResult::FAILURE_HANDLER_NULL:
      return true;
    case SafeBrowsingApiLookupResult::SUCCESS:
    case SafeBrowsingApiLookupResult::FAILURE:
    case SafeBrowsingApiLookupResult::FAILURE_API_CALL_TIMEOUT:
      return false;
  }
}

// Convert a SBThreatType to a Java SafetyNet API threat type.  We only support
// CSD_ALLOWLIST.
SafetyNetJavaThreatType SBThreatTypeToSafetyNetJavaThreatType(
    const SBThreatType& sb_threat_type) {
  using enum SBThreatType;
  CHECK(sb_threat_type == SB_THREAT_TYPE_CSD_ALLOWLIST);
  return SafetyNetJavaThreatType::CSD_ALLOWLIST;
}

// Convert a Java threat type for SafeBrowsing to a SBThreatType.
SBThreatType SafeBrowsingJavaToSBThreatType(
    SafeBrowsingJavaThreatType java_threat_num) {
  using enum SBThreatType;
  switch (java_threat_num) {
    case SafeBrowsingJavaThreatType::NO_THREAT:
      return SB_THREAT_TYPE_SAFE;
    case SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING:
      return SB_THREAT_TYPE_URL_PHISHING;
    case SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE:
      return SB_THREAT_TYPE_URL_UNWANTED;
    case SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION:
      return SB_THREAT_TYPE_URL_MALWARE;
    case SafeBrowsingJavaThreatType::BILLING:
      return SB_THREAT_TYPE_BILLING;
    case SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION:
    case SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION:
      return SB_THREAT_TYPE_SUBRESOURCE_FILTER;
  }
}

// Convert a SBThreatType to a Java threat type for SafeBrowsing. We only
// support a few.
SafeBrowsingJavaThreatType SBThreatTypeToSafeBrowsingApiJavaThreatType(
    const SBThreatType& sb_threat_type) {
  using enum SBThreatType;
  switch (sb_threat_type) {
    case SB_THREAT_TYPE_URL_PHISHING:
      return SafeBrowsingJavaThreatType::SOCIAL_ENGINEERING;
    case SB_THREAT_TYPE_URL_UNWANTED:
      return SafeBrowsingJavaThreatType::UNWANTED_SOFTWARE;
    case SB_THREAT_TYPE_URL_MALWARE:
      return SafeBrowsingJavaThreatType::POTENTIALLY_HARMFUL_APPLICATION;
    case SB_THREAT_TYPE_BILLING:
      return SafeBrowsingJavaThreatType::BILLING;
    default:
      NOTREACHED_IN_MIGRATION();
      return SafeBrowsingJavaThreatType::NO_THREAT;
  }
}

// Convert a vector of SBThreatTypes to JavaIntArray of SafeBrowsing API's
// threat types.
ScopedJavaLocalRef<jintArray> SBThreatTypeSetToSafeBrowsingJavaArray(
    JNIEnv* env,
    const SBThreatTypeSet& threat_types) {
  DCHECK_LT(0u, threat_types.size());
  size_t threat_type_size =
      base::Contains(threat_types,
                     SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER)
          ? threat_types.size() + 1
          : threat_types.size();
  auto int_threat_types = base::HeapArray<int>::WithSize(threat_type_size);
  auto itr = int_threat_types.begin();
  for (auto threat_type : threat_types) {
    if (threat_type == SBThreatType::SB_THREAT_TYPE_SUBRESOURCE_FILTER) {
      *itr++ = static_cast<int>(
          SafeBrowsingJavaThreatType::ABUSIVE_EXPERIENCE_VIOLATION);
      *itr++ =
          static_cast<int>(SafeBrowsingJavaThreatType::BETTER_ADS_VIOLATION);
    } else {
      *itr++ = static_cast<int>(
          SBThreatTypeToSafeBrowsingApiJavaThreatType(threat_type));
    }
  }
  return ToJavaIntArray(env, int_threat_types);
}

// Customized struct to hold a callback to the SafeBrowsing API and the protocol
// used to make that call. The protocol is stored for histogram logging.
struct SafeBrowsingResponseCallback {
  SafeBrowsingJavaProtocol protocol;
  SafeBrowsingApiHandlerBridge::ResponseCallback response_callback;
};

// The map that holds the callback_id used to reference each pending
// SafeBrowsing request sent to Java, and the corresponding callback to call on
// receiving the response.
using PendingSafeBrowsingCallbacksMap =
    std::unordered_map<jlong, SafeBrowsingResponseCallback>;

PendingSafeBrowsingCallbacksMap& GetPendingSafeBrowsingCallbacksMap() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Holds the list of callback objects that we are currently waiting to hear
  // the result of from GmsCore.
  // The key is a unique count-up integer.
  static base::NoDestructor<PendingSafeBrowsingCallbacksMap>
      pending_safe_browsing_callbacks;
  return *pending_safe_browsing_callbacks;
}

using PendingVerifyAppsCallbacksMap = std::unordered_map<
    jlong,
    SafeBrowsingApiHandlerBridge::VerifyAppsResponseCallback>;
PendingVerifyAppsCallbacksMap& GetPendingVerifyAppsCallbacks() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Holds the list of callback objects that we are currently waiting to hear
  // the result of from GmsCore.
  // The key is a unique count-up integer.
  static base::NoDestructor<PendingVerifyAppsCallbacksMap> pending_callbacks;
  return *pending_callbacks;
}

bool StartAllowlistCheck(const GURL& url, const SBThreatType& sb_threat_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
// stored in |pending_safe_browsing_callbacks|. Must be called on the original
// thread that starts the lookup.
void OnUrlCheckDoneBySafeBrowsingApi(
    jlong callback_id,
    SafeBrowsingApiLookupResult lookup_result,
    SafeBrowsingJavaThreatType threat_type,
    std::vector<int> threat_attributes,
    SafeBrowsingJavaResponseStatus response_status,
    jlong check_delta_microseconds) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  PendingSafeBrowsingCallbacksMap& pending_callbacks =
      GetPendingSafeBrowsingCallbacksMap();
  bool found = base::Contains(pending_callbacks, callback_id);
  DCHECK(found) << "Not found in pending_safe_browsing_callbacks: "
                << callback_id;
  if (!found) {
    return;
  }

  SafeBrowsingResponseCallback callback =
      std::move(pending_callbacks[callback_id]);
  pending_callbacks.erase(callback_id);

  ReportSafeBrowsingJavaResponse(callback.protocol, lookup_result, threat_type,
                                 threat_attributes, response_status,
                                 check_delta_microseconds);

  if (!IsResponseFromJavaValid(callback.protocol, lookup_result, threat_type,
                               threat_attributes, response_status)) {
    std::move(callback.response_callback)
        .Run(SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  if (!IsLookupSuccessful(lookup_result, response_status)) {
    if (IsSafeBrowsingNonRecoverable(lookup_result)) {
      SafeBrowsingApiHandlerBridge::GetInstance()
          .OnSafeBrowsingApiNonRecoverableFailure();
    }
    std::move(callback.response_callback)
        .Run(SBThreatType::SB_THREAT_TYPE_SAFE, ThreatMetadata());
    return;
  }

  std::move(callback.response_callback)
      .Run(
          SafeBrowsingJavaToSBThreatType(threat_type),
          GetThreatMetadataFromSafeBrowsingApi(threat_type, threat_attributes));
}

// Java->Native call, invoked when a SafeBrowsing check is done. |env| is the
// JNI environment that stores local pointers. |callback_id| is a key into the
// |pending_safe_browsing_callbacks| map, whose value is a ::ResponseCallback
// that will be called and then deleted on the IO thread. |j_lookup_result| is a
// @LookupResult from SafeBrowsingApiHandler.java. |j_threat_type| is the threat
// type that matched against the URL. |j_threat_attributes| is the threat
// attributes that matched against the URL. |j_response_status| reflects how the
// API gets the response. |check_delta_microseconds| is the number of
// microseconds it took to look up the URL reputation from GmsCore.
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
    jlong check_delta_microseconds) {
  SafeBrowsingApiLookupResult lookup_result =
      static_cast<SafeBrowsingApiLookupResult>(j_lookup_result);
  SafeBrowsingJavaThreatType threat_type =
      static_cast<SafeBrowsingJavaThreatType>(j_threat_type);
  std::vector<int> threat_attributes;
  JavaIntArrayToIntVector(env, j_threat_attributes, &threat_attributes);
  SafeBrowsingJavaResponseStatus response_status =
      static_cast<SafeBrowsingJavaResponseStatus>(j_response_status);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OnUrlCheckDoneBySafeBrowsingApi, callback_id,
                     lookup_result, threat_type, std::move(threat_attributes),
                     response_status, check_delta_microseconds));
}

void OnVerifyAppsEnabledDone(jlong callback_id, jint j_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  PendingVerifyAppsCallbacksMap& pending_callbacks =
      GetPendingVerifyAppsCallbacks();
  bool found = base::Contains(pending_callbacks, callback_id);
  DCHECK(found) << "Not found in pending_verify_apps_callbacks: "
                << callback_id;
  if (!found) {
    return;
  }

  SafeBrowsingApiHandlerBridge::VerifyAppsResponseCallback callback =
      std::move(pending_callbacks[callback_id]);
  std::move(callback).Run(static_cast<VerifyAppsEnabledResult>(j_result));
}

void JNI_SafeBrowsingApiBridge_OnVerifyAppsEnabledDone(JNIEnv* env,
                                                       jlong callback_id,
                                                       jint j_result) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&OnVerifyAppsEnabledDone, callback_id, j_result));
}

//
// SafeBrowsingApiHandlerBridge
//
SafeBrowsingApiHandlerBridge::SafeBrowsingApiHandlerBridge() {}
SafeBrowsingApiHandlerBridge::~SafeBrowsingApiHandlerBridge() {}

void SafeBrowsingApiHandlerBridge::ClearArtificialDatabase() {
  artificially_marked_phishing_urls_.clear();
}

void SafeBrowsingApiHandlerBridge::PopulateArtificialDatabase() {
  const std::string raw_artificial_urls =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kMarkAsPhishing);
  base::StringTokenizer tokenizer(raw_artificial_urls, ",");
  while (tokenizer.GetNext()) {
    auto candidate_url = GURL(tokenizer.token_piece());
    if (candidate_url.is_valid()) {
      artificially_marked_phishing_urls_.insert(candidate_url);
    }
  }
}

void SafeBrowsingApiHandlerBridge::StartHashDatabaseUrlCheck(
    ResponseCallback callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  bool for_browse_url = SBThreatTypeSetIsValidForCheckBrowseUrl(threat_types);
  if (for_browse_url &&
      base::Contains(threat_types, SBThreatType::SB_THREAT_TYPE_URL_PHISHING) &&
      base::Contains(artificially_marked_phishing_urls_, url)) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  SBThreatType::SB_THREAT_TYPE_URL_PHISHING,
                                  ThreatMetadata()));
    return;
  }
  StartUrlCheckBySafeBrowsing(std::move(callback), url, threat_types,
                              SafeBrowsingJavaProtocol::LOCAL_BLOCK_LIST);
}

void SafeBrowsingApiHandlerBridge::StartHashRealTimeUrlCheck(
    ResponseCallback callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types) {
  StartUrlCheckBySafeBrowsing(std::move(callback), url, threat_types,
                              SafeBrowsingJavaProtocol::REAL_TIME);
}

void SafeBrowsingApiHandlerBridge::StartUrlCheckBySafeBrowsing(
    ResponseCallback callback,
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    const SafeBrowsingJavaProtocol& protocol) {
  if (interceptor_for_testing_) {
    // For testing, only check the interceptor.
    interceptor_for_testing_->CheckBySafeBrowsing(std::move(callback), url);
    return;
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  base::UmaHistogramBoolean("SafeBrowsing.GmsSafeBrowsingApi.IsAvailable",
                            is_safe_browsing_api_available_);
  base::UmaHistogramBoolean("SafeBrowsing.GmsSafeBrowsingApi.IsAvailable" +
                                GetSafeBrowsingJavaProtocolUmaSuffix(protocol),
                            is_safe_browsing_api_available_);

  if (!is_safe_browsing_api_available_) {
    // Mark all requests as safe. Only users who have an old, broken GMSCore or
    // have sideloaded Chrome w/o PlayStore should land here.
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SBThreatType::SB_THREAT_TYPE_SAFE,
                       ThreatMetadata()));
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  jlong callback_id = next_safe_browsing_callback_id_++;
  auto safe_browsing_callback =
      SafeBrowsingResponseCallback(protocol, std::move(callback));
  GetPendingSafeBrowsingCallbacksMap().insert(
      {callback_id, std::move(safe_browsing_callback)});

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
  return StartAllowlistCheck(
      url, safe_browsing::SBThreatType::SB_THREAT_TYPE_CSD_ALLOWLIST);
}

void SafeBrowsingApiHandlerBridge::StartIsVerifyAppsEnabled(
    VerifyAppsResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (verify_apps_enabled_for_testing_.has_value()) {
    std::move(callback).Run(verify_apps_enabled_for_testing_.value());
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  if (!Java_SafeBrowsingApiBridge_ensureSafetyNetApiInitialized(env)) {
    std::move(callback).Run(VerifyAppsEnabledResult::FAILED);
    return;
  }

  jlong callback_id = next_verify_apps_callback_id_++;
  GetPendingVerifyAppsCallbacks().insert({callback_id, std::move(callback)});
  Java_SafeBrowsingApiBridge_isVerifyAppsEnabled(env, callback_id);
}
void SafeBrowsingApiHandlerBridge::StartEnableVerifyApps(
    VerifyAppsResponseCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  if (!Java_SafeBrowsingApiBridge_ensureSafetyNetApiInitialized(env)) {
    std::move(callback).Run(VerifyAppsEnabledResult::FAILED);
    return;
  }

  jlong callback_id = next_verify_apps_callback_id_++;
  GetPendingVerifyAppsCallbacks().insert({callback_id, std::move(callback)});
  Java_SafeBrowsingApiBridge_enableVerifyApps(env, callback_id);
}

void SafeBrowsingApiHandlerBridge::OnSafeBrowsingApiNonRecoverableFailure() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  is_safe_browsing_api_available_ = false;
}

}  // namespace safe_browsing
