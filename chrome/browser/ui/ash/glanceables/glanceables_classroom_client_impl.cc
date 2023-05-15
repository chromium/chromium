// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/glanceables_classroom_client_impl.h"

namespace ash {

GlanceablesClassroomClientImpl::GlanceablesClassroomClientImpl(
    const GlanceablesClassroomClientImpl::CreateRequestSenderCallback&
        create_request_sender_callback)
    : create_request_sender_callback_(create_request_sender_callback) {}

GlanceablesClassroomClientImpl::~GlanceablesClassroomClientImpl() = default;

}  // namespace ash
