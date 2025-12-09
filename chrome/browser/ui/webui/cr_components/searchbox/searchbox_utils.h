// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_

#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"

omnibox::ChromeAimEntryPoint PageClassificationToAimEntryPoint(
    ::metrics::OmniboxEventProto::PageClassification classification);

#endif  // CHROME_BROWSER_UI_WEBUI_CR_COMPONENTS_SEARCHBOX_SEARCHBOX_UTILS_H_
