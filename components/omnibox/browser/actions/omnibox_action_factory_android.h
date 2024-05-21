// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_FACTORY_ANDROID_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_FACTORY_ANDROID_H_

#include <vector>
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "components/omnibox/browser/actions/omnibox_action.h"
#include "components/omnibox/browser/actions/omnibox_pedal_concepts.h"

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint,
    OmniboxPedalId pedal_id);

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint,
    const std::string& query);

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxActionInSuggest(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint,
    int action_type,
    const std::string& action_uri);

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxAnswerAction(
    JNIEnv* env,
    intptr_t instance,
    const std::u16string& hint,
    const std::u16string& accessibility_hint);

std::vector<jni_zero::ScopedJavaLocalRef<jobject>> ToJavaOmniboxActionsList(
    JNIEnv* env,
    const std::vector<scoped_refptr<OmniboxAction>>& actions);

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_ACTION_FACTORY_ANDROID_H_
