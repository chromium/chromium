// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/history_ui_android.h"

#include "base/memory/ref_counted_memory.h"
#include "components/grit/components_scaled_resources.h"
#include "ui/base/resource/resource_bundle.h"

// static
base::RefCountedMemory* HistoryUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_HISTORY_FAVICON, scale_factor);
}
