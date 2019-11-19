// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_UTIL_H_

#include "url/gurl.h"

// This file contains some useful utilities for the safe_browsing/triggers
// classes
namespace content {
class RenderFrameHost;
}

namespace safe_browsing {

// Based on heuristics, guesses whether |render_frame_host| is showing a Google
// Ad, or the |frame_url| is a Google Ad URL.
bool DetectGoogleAd(content::RenderFrameHost* render_frame_host,
                    const GURL& frame_url);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_TRIGGERS_TRIGGER_UTIL_H_
