// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_AGENT_UTIL_H_
#define COMPONENTS_UI_DEVTOOLS_AGENT_UTIL_H_

#include <string>

#include "components/ui_devtools/devtools_export.h"

namespace ui_devtools {

UI_DEVTOOLS_EXPORT extern const char kChromiumCodeSearchURL[];
UI_DEVTOOLS_EXPORT extern const char kChromiumCodeSearchSrcURL[];

// Synchonously gets source code and returns true if successful.
bool UI_DEVTOOLS_EXPORT GetSourceCode(std::string path,
                                      std::string* source_code);

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_AGENT_UTIL_H_
