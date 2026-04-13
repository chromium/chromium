// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmarks_ui_android.h"

#include "base/memory/ref_counted_memory.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

// static
base::RefCountedMemory* BookmarksUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_BOOKMARKS_FAVICON, scale_factor);
}
