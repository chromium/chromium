// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_UTILS_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_UTILS_H_

#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace memory_saver {

// Returns whether |url| supports showing discard indicators
bool IsURLSupported(GURL url);

// Returns the discard reason if |contents| has been discarded
std::optional<::mojom::LifecycleUnitDiscardReason> GetDiscardReason(
    content::WebContents* contents);

// Returns how much memory was saved through discarding `contents`
int64_t GetDiscardedMemorySavingsInBytes(content::WebContents* contents);

}  // namespace memory_saver

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_MEMORY_SAVER_UTILS_H_
