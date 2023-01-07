// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/view_handler_android.h"

namespace autofill_assistant {

ViewHandlerAndroid::ViewHandlerAndroid() = default;
ViewHandlerAndroid::~ViewHandlerAndroid() = default;

base::WeakPtr<ViewHandlerAndroid> ViewHandlerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

absl::optional<base::android::ScopedJavaGlobalRef<jobject>>
ViewHandlerAndroid::GetView(const std::string& view_identifier) const {
  auto it = views_.find(view_identifier);
  if (it == views_.end()) {
    return absl::nullopt;
  }
  return it->second;
}

// Adds a view to the set of managed views.
void ViewHandlerAndroid::AddView(
    const std::string& view_identifier,
    base::android::ScopedJavaGlobalRef<jobject> jview) {
  DCHECK(views_.find(view_identifier) == views_.end());
  views_.emplace(view_identifier, jview);
}

}  // namespace autofill_assistant
