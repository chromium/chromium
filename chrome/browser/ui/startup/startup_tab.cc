// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab.h"

#include <utility>

StartupTab::StartupTab(GURL url, Type type) : url(std::move(url)), type(type) {}

StartupTab::StartupTab(GURL url, bool is_untrusted_launch)
    : url(std::move(url)), is_untrusted_launch(is_untrusted_launch) {}

StartupTab::~StartupTab() = default;
