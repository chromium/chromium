// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/ukm/android/jni_headers/UkmRecorder_jni.h"

namespace metrics {

struct MetricFromJava {
  std::string name;
  int value;
};

MetricFromJava ConvertJavaMetric(
    JNIEnv* env,
    jni_zero::ScopedJavaLocalRef<jobject> j_metric) {
  MetricFromJava metric;
  metric.name = base::android::ConvertJavaStringToUTF8(
      env, Java_UkmRecorder_getNameFromMetric(env, j_metric));
  metric.value = Java_UkmRecorder_getValueFromMetric(env, j_metric);
  return metric;
}

void ConvertJavaMetricsArrayToVector(
    JNIEnv* env,
    const base::android::JavaRef<jobjectArray>& array,
    std::vector<MetricFromJava>* out) {
  if (!array) {
    return;
  }

  jsize jlength = env->GetArrayLength(array.obj());
  // GetArrayLength() returns -1 if |array| is not a valid Java array.
  CHECK_GE(jlength, 0) << "Invalid array length: " << jlength;
  size_t length = static_cast<size_t>(std::max(0, jlength));
  for (size_t i = 0; i < length; ++i) {
    jni_zero::ScopedJavaLocalRef<jobject> j_metric(
        env, static_cast<jobject>(env->GetObjectArrayElement(array.obj(), i)));
    out->emplace_back(ConvertJavaMetric(env, j_metric));
  }
}

// Called by Java org.chromium.chrome.browser.metrics.UkmRecorder.
static void JNI_UkmRecorder_RecordEventWithMultipleMetrics(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_event_name,
    const base::android::JavaParamRef<jobjectArray>& j_metrics) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const std::string event_name(
      base::android::ConvertJavaStringToUTF8(env, j_event_name));
  const ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  std::vector<MetricFromJava> metrics;
  ConvertJavaMetricsArrayToVector(env, j_metrics, &metrics);

  ukm::UkmEntryBuilder builder(source_id, event_name);
  for (auto& metric : metrics) {
    builder.SetMetric(std::move(metric.name), metric.value);
  }
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace metrics
