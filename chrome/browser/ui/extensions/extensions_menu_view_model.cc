// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"

ExtensionsMenuViewModel::ExtensionsMenuViewModel(
    std::unique_ptr<ExtensionsMenuViewPlatformDelegate> platform_delegate)
    : platform_delegate_(std::move(platform_delegate)) {}

ExtensionsMenuViewModel::~ExtensionsMenuViewModel() = default;
