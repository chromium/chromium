// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_H_
#define COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_H_

#include <map>
#include <vector>

namespace cached_flags {

// This class allows us to mock the actual JNI calls.
// The implementation should perform no work other than JNI calls.
class JniDelegate {
 public:
  virtual ~JniDelegate() = default;

  virtual void CacheNativeFlagsImmediately(
      const std::map<std::string, std::string>& features) = 0;

  virtual void CacheFeatureParamsImmediately(
      const std::map<std::string, std::map<std::string, std::string>>&
          feature_params) = 0;

  virtual void EraseNativeFlagCachedValues(
      const std::vector<std::string>& features_to_erase) = 0;

  virtual void EraseFeatureParamCachedValues(
      const std::vector<std::string>& feature_params_to_erase) = 0;
};

}  // namespace cached_flags

#endif  // COMPONENTS_CACHED_FLAGS_ANDROID_JNI_DELEGATE_H_
