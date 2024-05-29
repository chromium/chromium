// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_entry_builder.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/ukm/android/jni_headers/UkmRecorder_jni.h"

namespace metrics {

// Called by Java org.chromium.chrome.browser.metrics.UkmRecorder.
static void JNI_UkmRecorder_RecordEventWithBooleanMetric(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_event_name,
    const base::android::JavaParamRef<jstring>& j_metric_name) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  const std::string event_name(
      base::android::ConvertJavaStringToUTF8(env, j_event_name));
  ukm::UkmEntryBuilder builder(source_id, event_name);
  builder.SetMetric(base::android::ConvertJavaStringToUTF8(env, j_metric_name),
                    true);
  builder.Record(ukm::UkmRecorder::Get());
}

// Called by Java org.chromium.chrome.browser.metrics.UkmRecorder.
static void JNI_UkmRecorder_RecordEventWithIntegerMetric(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    const base::android::JavaParamRef<jstring>& j_event_name,
    const base::android::JavaParamRef<jstring>& j_metric_name,
    jint j_metric_value) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  const ukm::SourceId source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();
  const std::string event_name(
      base::android::ConvertJavaStringToUTF8(env, j_event_name));
  ukm::UkmEntryBuilder builder(source_id, event_name);
  builder.SetMetric(base::android::ConvertJavaStringToUTF8(env, j_metric_name),
                    j_metric_value);
  builder.Record(ukm::UkmRecorder::Get());
}

}  // namespace metrics
