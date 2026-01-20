// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/global_browser_collection_platform_delegate.h"

GlobalBrowserCollectionPlatformDelegate::
    GlobalBrowserCollectionPlatformDelegate(GlobalBrowserCollection& parent)
    : parent_(parent) {}

GlobalBrowserCollectionPlatformDelegate::
    ~GlobalBrowserCollectionPlatformDelegate() = default;
