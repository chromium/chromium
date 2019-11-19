// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/service/cast_service.h"

#include "base/logging.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"

namespace chromecast {

CastService::CastService(
    content::BrowserContext* browser_context,
    PrefService* pref_service)
    : browser_context_(browser_context),
      pref_service_(pref_service),
      stopped_(true),
      thread_checker_(new base::ThreadChecker()) {
}

CastService::~CastService() {
  DCHECK(thread_checker_->CalledOnValidThread());
  DCHECK(stopped_);
}

void CastService::Initialize() {
  DCHECK(thread_checker_->CalledOnValidThread());
  InitializeInternal();
}

void CastService::Finalize() {
  DCHECK(thread_checker_->CalledOnValidThread());
  FinalizeInternal();
}

void CastService::Start() {
  DCHECK(thread_checker_->CalledOnValidThread());
  stopped_ = false;
  StartInternal();
}

void CastService::Stop() {
  DCHECK(thread_checker_->CalledOnValidThread());
  StopInternal();
  // Consume any pending tasks which should be done before destroying in-process
  // renderer process, for example, destroying web_contents.
  base::RunLoop().RunUntilIdle();
  stopped_ = true;
}

void CastService::AccessibilityStateChanged(bool enabled) {
  NOTIMPLEMENTED();
}

}  // namespace chromecast
