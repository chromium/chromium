// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/event_handler.h"
#include "base/strings/string_number_conversions.h"

#include "base/logging.h"

namespace autofill_assistant {

EventHandler::EventHandler() = default;
EventHandler::~EventHandler() = default;

void EventHandler::DispatchEvent(const EventKey& key) {
  DVLOG(3) << __func__ << " " << key;
  for (auto& observer : observers_) {
    observer.OnEvent(key);
  }
}

void EventHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void EventHandler::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
absl::optional<EventHandler::EventKey> EventHandler::CreateEventKeyFromProto(
    const EventProto& proto) {
  switch (proto.kind_case()) {
    case EventProto::kOnValueChanged:
      if (proto.on_value_changed().model_identifier().empty()) {
        VLOG(1) << "Invalid OnValueChangedEventProto: no model_identifier "
                   "specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_value_changed().model_identifier()});
    case EventProto::kOnViewClicked:
      if (proto.on_view_clicked().view_identifier().empty()) {
        VLOG(1) << "Invalid OnViewClickedEventProto: no view_identifier "
                   "specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_view_clicked().view_identifier()});
    case EventProto::kOnUserActionCalled:
      if (proto.on_user_action_called().user_action_identifier().empty()) {
        VLOG(1) << "Invalid OnUserActionCalled: no user_action_identifier "
                   "specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(),
           proto.on_user_action_called().user_action_identifier()});
    case EventProto::kOnTextLinkClicked:
      if (!proto.on_text_link_clicked().has_text_link()) {
        VLOG(1) << "Invalid OnTextLinkClickedProto: no text_link specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(),
           base::NumberToString(proto.on_text_link_clicked().text_link())});
    case EventProto::kOnPopupDismissed:
      if (proto.on_popup_dismissed().popup_identifier().empty()) {
        VLOG(1)
            << "Invalid OnPopupDismissedProto: no popup_identifier specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(), proto.on_popup_dismissed().popup_identifier()});
    case EventProto::kOnViewContainerCleared:
      if (proto.on_view_container_cleared().view_identifier().empty()) {
        VLOG(1) << "Invalid OnViewContainerClearedProto: no view_identifier "
                   "specified";
        return absl::nullopt;
      }
      return absl::optional<EventHandler::EventKey>(
          {proto.kind_case(),
           proto.on_view_container_cleared().view_identifier()});
    case EventProto::KIND_NOT_SET:
      VLOG(1) << "Error creating event: kind not set";
      return absl::nullopt;
  }
}

std::ostream& operator<<(std::ostream& out,
                         const EventProto::KindCase& event_case) {
#ifdef NDEBUG
  out << static_cast<int>(event_case);
  return out;
#else
  switch (event_case) {
    case EventProto::kOnValueChanged:
      out << "kOnValueChanged";
      break;
    case EventProto::kOnViewClicked:
      out << "kOnViewClicked";
      break;
    case EventProto::kOnUserActionCalled:
      out << "kOnUserActionCalled";
      break;
    case EventProto::kOnTextLinkClicked:
      out << "kOnTextLinkClicked";
      break;
    case EventProto::kOnPopupDismissed:
      out << "kOnPopupDismissed";
      break;
    case EventProto::kOnViewContainerCleared:
      out << "kOnViewContainerCleared";
      break;
    case EventProto::KIND_NOT_SET:
      break;
  }
  return out;
#endif
}

std::ostream& operator<<(std::ostream& out, const EventHandler::EventKey& key) {
  out << "{" << key.first << ", " << key.second << "}";
  return out;
}

}  // namespace autofill_assistant
