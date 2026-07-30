// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_ANDROID_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_ANDROID_H_

#include "base/memory/scoped_refptr.h"
#include "ui/base/resource/resource_scale_factor.h"

namespace base {
class RefCountedMemory;
}

class HistoryUI {
 public:
  HistoryUI() = delete;
  HistoryUI(const HistoryUI&) = delete;
  HistoryUI& operator=(const HistoryUI&) = delete;

  static scoped_refptr<base::RefCountedMemory> GetFaviconResourceBytes(
      ui::ResourceScaleFactor scale_factor);
};

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_HISTORY_UI_ANDROID_H_
