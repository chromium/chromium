// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_EVENTS_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_EVENTS_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"

namespace autofill_assistant {
class ViewHandlerAndroid;

namespace android_events {

// Creates java listeners for all view events in |proto| such that |jdelegate|
// is notified when appropriate (e.g., OnClickListeners). Returns true on
// success, false on failure.
bool CreateJavaListenersFromProto(
    JNIEnv* env,
    ViewHandlerAndroid* view_handler,
    base::android::ScopedJavaGlobalRef<jobject> jdelegate,
    const InteractionsProto& proto);

}  // namespace android_events
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_GENERIC_UI_EVENTS_ANDROID_H_
