// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_H_
#define CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_H_

#include "build/build_config.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/views/javascript_app_modal_event_blocker_mac.h"

using JavascriptAppModalEventBlocker = JavascriptAppModalEventBlockerMac;
#else
#include "chrome/browser/ui/views/javascript_app_modal_event_blocker_aura.h"

using JavascriptAppModalEventBlocker = JavascriptAppModalEventBlockerAura;
#endif

#endif  // CHROME_BROWSER_UI_VIEWS_JAVASCRIPT_APP_MODAL_EVENT_BLOCKER_H_
