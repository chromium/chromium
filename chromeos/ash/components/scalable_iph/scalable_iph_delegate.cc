// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

ScalableIphDelegate::NotificationParams::NotificationParams() = default;

ScalableIphDelegate::NotificationParams::NotificationParams(
    const NotificationParams&) = default;

ScalableIphDelegate::NotificationParams&
ScalableIphDelegate::NotificationParams::NotificationParams::operator=(
    const NotificationParams&) = default;

ScalableIphDelegate::NotificationParams::~NotificationParams() = default;

}  // namespace scalable_iph
