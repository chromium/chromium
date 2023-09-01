// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"

namespace scalable_iph {

ScalableIphDelegate::BubbleParams::BubbleParams() = default;
ScalableIphDelegate::BubbleParams::BubbleParams(const BubbleParams&) = default;
ScalableIphDelegate::BubbleParams&
ScalableIphDelegate::BubbleParams::BubbleParams::operator=(
    const BubbleParams&) = default;
ScalableIphDelegate::BubbleParams::~BubbleParams() = default;

ScalableIphDelegate::NotificationParams::NotificationParams() = default;
ScalableIphDelegate::NotificationParams::NotificationParams(
    const NotificationParams&) = default;
ScalableIphDelegate::NotificationParams&
ScalableIphDelegate::NotificationParams::NotificationParams::operator=(
    const NotificationParams&) = default;
ScalableIphDelegate::NotificationParams::~NotificationParams() = default;

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::SessionState session_state) {
  switch (session_state) {
    case ScalableIphDelegate::SessionState::kUnknownInitialValue:
      return out << "UnknownInitialValue";
    case ScalableIphDelegate::SessionState::kActive:
      return out << "Active";
    case ScalableIphDelegate::SessionState::kLocked:
      return out << "Locked";
    case ScalableIphDelegate::SessionState::kOther:
      return out << "Other";
  }
}

}  // namespace scalable_iph
