// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_IMPL_H_
#define COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_IMPL_H_

#include "jni_delegate.h"

namespace cached_flags {

// An implementation of JniDelegate, which makes the appropriate JNI calls.
class JniDelegateImpl : public JniDelegate {
 public:
  ~JniDelegateImpl() override;

  void CacheNativeFlagsImmediately(
      const std::map<std::string, std::string>& features) override;

  void CacheFeatureParamsImmediately(
      const std::map<std::string, std::map<std::string, std::string>>&
          feature_params) override;

  void EraseNativeFlagCachedValues(
      const std::vector<std::string>& features_to_erase) override;

  void EraseFeatureParamCachedValues(
      const std::vector<std::string>& feature_params_to_erase) override;
};

}  // namespace cached_flags

#endif  // COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_IMPL_H_
