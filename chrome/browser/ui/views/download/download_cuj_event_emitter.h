// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_CUJ_EVENT_EMITTER_H_
#define CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_CUJ_EVENT_EMITTER_H_

#include "ui/base/interaction/element_tracker.h"

class BrowserWindowInterface;

namespace download {

// Emits `event_type` against the BrowserView of `browser`'s window so the
// element tracker can route it to subscribers (e.g. the critical user
// journey registry). Returns true if the event was emitted.
bool EmitElementTrackerEvent(BrowserWindowInterface* browser,
                             ui::CustomElementEventType event_type);

}  // namespace download

#endif  // CHROME_BROWSER_UI_VIEWS_DOWNLOAD_DOWNLOAD_CUJ_EVENT_EMITTER_H_
