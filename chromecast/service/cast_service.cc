// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/service/cast_service.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/threading/thread_checker.h"

namespace chromecast {

CastService::CastService()
    : stopped_(true), thread_checker_(new base::ThreadChecker()) {}

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

}  // namespace chromecast
