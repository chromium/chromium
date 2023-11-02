// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/multidevice/secure_message_delegate.h"

namespace ash::multidevice {

SecureMessageDelegate::SecureMessageDelegate() = default;

SecureMessageDelegate::~SecureMessageDelegate() = default;

SecureMessageDelegate::CreateOptions::CreateOptions() = default;

SecureMessageDelegate::CreateOptions::CreateOptions(
    const CreateOptions& other) = default;

SecureMessageDelegate::CreateOptions::~CreateOptions() = default;

SecureMessageDelegate::UnwrapOptions::UnwrapOptions() = default;

SecureMessageDelegate::UnwrapOptions::~UnwrapOptions() = default;

}  // namespace ash::multidevice
