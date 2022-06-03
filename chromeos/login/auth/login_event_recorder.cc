// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/login_event_recorder.h"

#include <vector>

#include "base/lazy_instance.h"

namespace chromeos {

static base::LazyInstance<LoginEventRecorder>::DestructorAtExit
    g_login_event_recorder = LAZY_INSTANCE_INITIALIZER;

LoginEventRecorder::LoginEventRecorder() : delegate_(NULL) {}

LoginEventRecorder::~LoginEventRecorder() = default;

// static
LoginEventRecorder* LoginEventRecorder::Get() {
  return g_login_event_recorder.Pointer();
}

void LoginEventRecorder::SetDelegate(LoginEventRecorder::Delegate* delegate) {
  delegate_ = delegate;
}

void LoginEventRecorder::AddLoginTimeMarker(const char* marker_name,
                                            bool send_to_uma) {
  if (delegate_)
    delegate_->AddLoginTimeMarker(marker_name, send_to_uma);
}

void LoginEventRecorder::RecordAuthenticationSuccess() {
  if (delegate_)
    delegate_->RecordAuthenticationSuccess();
}

void LoginEventRecorder::RecordAuthenticationFailure() {
  if (delegate_)
    delegate_->RecordAuthenticationFailure();
}

}  // namespace chromeos
