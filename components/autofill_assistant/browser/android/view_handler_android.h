// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_VIEW_HANDLER_ANDROID_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_VIEW_HANDLER_ANDROID_H_

#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Manages a map of view-identifier -> android view instances.
class ViewHandlerAndroid {
 public:
  ViewHandlerAndroid();
  ~ViewHandlerAndroid();
  ViewHandlerAndroid(const ViewHandlerAndroid&) = delete;
  ViewHandlerAndroid& operator=(const ViewHandlerAndroid&) = delete;

  base::WeakPtr<ViewHandlerAndroid> GetWeakPtr();

  // Returns the view associated with |view_identifier| or absl::nullopt if
  // there is no such view.
  absl::optional<base::android::ScopedJavaGlobalRef<jobject>> GetView(
      const std::string& view_identifier) const;

  // Adds a view to the set of managed views.
  void AddView(const std::string& view_identifier,
               base::android::ScopedJavaGlobalRef<jobject> jview);

 private:
  base::flat_map<std::string, base::android::ScopedJavaGlobalRef<jobject>>
      views_;
  base::WeakPtrFactory<ViewHandlerAndroid> weak_ptr_factory_{this};
};

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_VIEW_HANDLER_ANDROID_H_
