// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/embedder_support/android/metrics/android_metrics_log_uploader.h"

#include "base/android/jni_array.h"
#include "base/task/thread_pool.h"
#include "components/embedder_support/android/metrics/features.h"
#include "components/metrics/log_decoder.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/embedder_support/android/metrics/jni/AndroidMetricsLogUploader_jni.h"

using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace metrics {

AndroidMetricsLogUploader::AndroidMetricsLogUploader(
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : on_upload_complete_(on_upload_complete) {}

AndroidMetricsLogUploader::~AndroidMetricsLogUploader() = default;

int32_t UploadLogWithUploader(const std::string& log_data,
                              const bool async_metric_logging_feature) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> java_data = ToJavaByteArray(env, log_data);

  return Java_AndroidMetricsLogUploader_uploadLog(env, java_data,
                                                  async_metric_logging_feature);
}

void AndroidMetricsLogUploader::UploadLog(
    const std::string& compressed_log_data,
    const LogMetadata& /*log_metadata*/,
    const std::string& /*log_hash*/,
    const std::string& /*log_signature*/,
    const ReportingInfo& reporting_info) {
  // This uploader uses the platform logging mechanism instead of the normal UMA
  // server. The platform mechanism does its own compression, so undo the
  // previous compression.
  std::string log_data;
  if (!DecodeLogData(compressed_log_data, &log_data)) {
    // If the log is corrupt, pretend the server rejected it (HTTP Bad Request).
    OnUploadComplete(400);
    return;
  }

  if (base::FeatureList::IsEnabled(kAndroidMetricsAsyncMetricLogging)) {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&UploadLogWithUploader, log_data, true),
        base::BindOnce(&AndroidMetricsLogUploader::OnUploadComplete,
                       weak_factory_.GetWeakPtr()));
  } else {
    UploadLogWithUploader(log_data, false);

    // Just pass 200 (HTTP OK) and pretend everything is peachy.
    OnUploadComplete(200);
  }
}

void AndroidMetricsLogUploader::OnUploadComplete(const int32_t status) {
  on_upload_complete_.Run(status, /*error_code=*/0, /*was_https=*/true,
                          /*force_discard=*/false, /*force_discard_reason=*/"");
}

}  // namespace metrics
