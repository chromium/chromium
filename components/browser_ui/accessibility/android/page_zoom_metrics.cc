// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_ui/accessibility/android/page_zoom_metrics.h"

#include "base/android/scoped_java_ref.h"
#include "components/browser_ui/accessibility/android/accessibility_jni_headers/PageZoomMetrics_jni.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace browser_ui {

using base::android::JavaParamRef;
using base::android::JavaRef;

void JNI_PageZoomMetrics_LogZoomLevelUKM(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& j_web_contents,
    jdouble newZoomLevel) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);
  DCHECK(web_contents);

  ukm::SourceId ukm_source_id =
      web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId();

  ukm::builders::Accessibility_PageZoom(ukm_source_id)
      .SetSliderZoomValue((int)round(100 * newZoomLevel))
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace browser_ui
