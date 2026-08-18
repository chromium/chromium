// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fcm/fcm_message.h"

namespace fcm {

FcmMessage::FcmMessage() = default;

FcmMessage::FcmMessage(const FcmMessage& other) = default;

FcmMessage& FcmMessage::operator=(const FcmMessage& other) = default;

FcmMessage::FcmMessage(FcmMessage&& other) = default;

FcmMessage& FcmMessage::operator=(FcmMessage&& other) = default;

FcmMessage::~FcmMessage() = default;

}  // namespace fcm
