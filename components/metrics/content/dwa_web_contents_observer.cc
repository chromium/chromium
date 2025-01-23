// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/content/dwa_web_contents_observer.h"

#include "components/metrics/dwa/dwa_recorder.h"

namespace metrics {

DwaWebContentsObserver::DwaWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<DwaWebContentsObserver>(*web_contents) {}

DwaWebContentsObserver::~DwaWebContentsObserver() = default;

void DwaWebContentsObserver::DidStopLoading() {
  dwa::DwaRecorder::Get()->OnPageLoad();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(DwaWebContentsObserver);  // nocheck

}  // namespace metrics
