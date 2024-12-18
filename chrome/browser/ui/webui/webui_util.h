// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include "base/containers/span.h"
#include "build/build_config.h"
#include "ui/base/webui/resource_path.h"
#include "ui/webui/webui_util.h"

using webui::AddLocalizedString;
using webui::EnableTrustedTypesCSP;
using webui::kDefaultTrustedTypesPolicies;
using webui::SetJSModuleDefaults;
using webui::SetupWebUIDataSource;

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
