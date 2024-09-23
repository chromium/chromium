// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/metrics/histogram_macros.h"
#include "sandbox/linux/seccomp-bpf-helpers/seccomp_starter_android.h"
#include "sandbox/sandbox_buildflags.h"

#if BUILDFLAG(USE_SECCOMP_BPF)
#include "sandbox/linux/seccomp-bpf-helpers/baseline_policy_android.h"
#endif

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/browser_ui/photo_picker/android/photo_picker_jni_headers/ImageDecoder_jni.h"

void JNI_ImageDecoder_InitializePhotoPickerSandbox(JNIEnv* env) {
  auto* info = base::android::BuildInfo::GetInstance();
  sandbox::SeccompStarterAndroid starter(info->sdk_int());

#if BUILDFLAG(USE_SECCOMP_BPF)
  // The policy compiler is only available if USE_SECCOMP_BPF is enabled.
  starter.set_policy(std::make_unique<sandbox::BaselinePolicyAndroid>(
      starter.GetDefaultBaselineOptions()));
#endif
  starter.StartSandbox();
}
