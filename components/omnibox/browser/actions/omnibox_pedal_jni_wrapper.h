// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_
#define COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_

base::android::ScopedJavaGlobalRef<jobject> BuildOmniboxPedal(
    int id,
    std::u16string hint,
    std::u16string suggestion_contents,
    std::u16string accessibility_suffix,
    std::u16string accessibility_hint,
    GURL url);

base::android::ScopedJavaGlobalRef<jobject> BuildHistoryClustersAction(
    int id,
    std::u16string hint,
    std::u16string suggestion_contents,
    std::u16string accessibility_suffix,
    std::u16string accessibility_hint,
    GURL url,
    std::string query);

#endif  // COMPONENTS_OMNIBOX_BROWSER_ACTIONS_OMNIBOX_PEDAL_JNI_WRAPPER_H_
