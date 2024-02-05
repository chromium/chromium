// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/scalable_iph/scalable_iph_delegate.h"
#include "chromeos/ash/components/scalable_iph/scalable_iph_constants.h"

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

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::Action action) {
  return out << "Action: action_type: " << action.action_type
             << " iph_event_name: " << action.iph_event_name;
}

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::Button button) {
  return out << "Button: text: " << button.text << " action: (" << button.action
             << ")";
}

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::BubbleIcon bubble_icon) {
  switch (bubble_icon) {
    case ScalableIphDelegate::BubbleIcon::kNoIcon:
      return out << "NoIcon";
    case ScalableIphDelegate::BubbleIcon::kChromeIcon:
      return out << "ChromeIcon";
    case ScalableIphDelegate::BubbleIcon::kPlayStoreIcon:
      return out << "PlayStoreIcon";
    case ScalableIphDelegate::BubbleIcon::kGoogleDocsIcon:
      return out << "GoogleDocsIcon";
    case ScalableIphDelegate::BubbleIcon::kGooglePhotosIcon:
      return out << "GooglePhotosIcon";
    case ScalableIphDelegate::BubbleIcon::kPrintJobsIcon:
      return out << "PrintJobsIcon";
    case ScalableIphDelegate::BubbleIcon::kYouTubeIcon:
      return out << "YouTubeIcon";
  }
}

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::BubbleParams params) {
  return out << "BubbleParams: bubble_id: " << params.bubble_id
             << " title: " << params.title << " text: " << params.text
             << " icon: " << params.icon << " button: (" << params.button
             << ") anchor_view_app_id: " << params.anchor_view_app_id;
}

std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationIcon notification_icon) {
  switch (notification_icon) {
    case ScalableIphDelegate::NotificationIcon::kDefault:
      return out << "Default";
    case ScalableIphDelegate::NotificationIcon::kRedeem:
      return out << "Redeem";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationSummaryText notification_summary_text) {
  switch (notification_summary_text) {
    case ScalableIphDelegate::NotificationSummaryText::kNone:
      return out << "None";
    case ScalableIphDelegate::NotificationSummaryText::kWelcomeTips:
      return out << "WelcomeTips";
  }
}

std::ostream& operator<<(
    std::ostream& out,
    ScalableIphDelegate::NotificationImageType notification_image_type) {
  switch (notification_image_type) {
    case ScalableIphDelegate::NotificationImageType::kNoImage:
      return out << "NoImage";
    case ScalableIphDelegate::NotificationImageType::kWallpaper:
      return out << kCustomNotificationImageTypeValueWallpaper;
    case ScalableIphDelegate::NotificationImageType::kMinecraft:
      return out << kCustomNotificationImageTypeValueMinecraft;
  }
}

std::ostream& operator<<(std::ostream& out,
                         ScalableIphDelegate::NotificationParams params) {
  return out << "NotificationParams: notification_id: "
             << params.notification_id << " icon: " << params.icon
             << " source: " << params.source
             << " summary_text: " << params.summary_text
             << " title: " << params.title << " text: " << params.text
             << " image_type: " << params.image_type << " button: ("
             << params.button << ")";
}

}  // namespace scalable_iph
