// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_

#include <vector>

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    JNIEnv* env,
    const std::u16string& hint,
    OmniboxPedalId pedal_id);

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    JNIEnv* env,
    const std::u16string& hint,
    const std::string& query);

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxActionInSuggest(
    JNIEnv* env,
    const std::u16string& hint,
    const std::string& serialized_action);

base::android::ScopedJavaLocalRef<jobjectArray> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions);

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_
