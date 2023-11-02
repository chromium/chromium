// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/startup_tab.h"

StartupTab::StartupTab(const GURL& url, Type type) : url(url), type(type) {}

StartupTab::~StartupTab() = default;
