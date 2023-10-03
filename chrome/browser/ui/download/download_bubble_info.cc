// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_bubble_info.h"

DownloadBubbleInfoChangeObserver::DownloadBubbleInfoChangeObserver() = default;

DownloadBubbleInfoChangeObserver::~DownloadBubbleInfoChangeObserver() {
  CHECK(!IsInObserverList());
}
