// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/bind.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/android/features/start_surface/jni_headers/StartSurfaceConfiguration_jni.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/ui/android/start_surface/start_surface_android.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using base::android::JavaParamRef;

namespace {
int SPARE_RENDERER_DELAY_MS = 1000;

void WarmUpRenderProcess(Profile* profile) {
  // It is ok for this to get called multiple times since all the requests
  // will get de-duplicated to the first one.
  content::RenderProcessHost::WarmupSpareRenderProcessHost(profile);
}

}  // namespace

bool IsStartSurfaceBehaviouralTargetingEnabled() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_StartSurfaceConfiguration_isBehaviouralTargetingEnabled(env);
}

static void JNI_StartSurfaceConfiguration_WarmupRenderer(
    JNIEnv* env,
    const JavaParamRef<jobject>& jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  auto renderer_delay_ms = base::GetFieldTrialParamByFeatureAsInt(
      chrome::android::kStartSurfaceAndroid, "spare_renderer_delay_ms",
      SPARE_RENDERER_DELAY_MS);

  content::GetUIThreadTaskRunner({})->PostDelayedTask(
      FROM_HERE, base::BindOnce(&WarmUpRenderProcess, profile),
      base::Milliseconds(renderer_delay_ms));
}
