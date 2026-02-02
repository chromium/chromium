// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_search_feature.h"

#include <string>

#include "chrome/browser/glic/public/glic_enabling.h"

namespace features {
bool HasTabSearchToolbarButton() {
  // It is important that this value not change at runtime in production. Any
  // future updates to this function must maintain that property.
  return glic::GlicEnabling::IsEnabledByFlags();
}
}  // namespace features
