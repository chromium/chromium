// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/accessibility/android/page_zoom_metrics.h"

#include "base/android/scoped_java_ref.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/accessibility/android/accessibility_jni_headers/PageZoomMetrics_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

namespace {
int ValueToStep(double value) {
  return (static_cast<int>(round(100 * value)) / 5) * 5;
}
}  // namespace

namespace browser_ui {

void JNI_PageZoomMetrics_LogZoomLevelUKM(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jdouble new_zoom_level) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  ukm::SourceId ukm_source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  PageZoomMetrics::LogZoomLevelUKMHelper(ukm_source_id, new_zoom_level,
                                         ukm::UkmRecorder::Get());
}

// static
void PageZoomMetrics::LogZoomLevelUKMHelper(ukm::SourceId ukm_source_id,
                                            double new_zoom_level,
                                            ukm::UkmRecorder* ukm_recorder) {
  ukm::builders::Accessibility_PageZoom(ukm_source_id)
      .SetSliderZoomValue(ValueToStep(new_zoom_level))
      .Record(ukm_recorder);
}

}  // namespace browser_ui
