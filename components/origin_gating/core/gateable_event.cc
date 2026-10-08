// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/gateable_event.h"

#include <string>
#include <utility>
#include <variant>

#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace origin_gating {

GateableEvent::GateableEvent(NavigationRequestEvent event)
    : data_(std::move(event)) {}

GateableEvent::GateableEvent(NavigationResponseEvent event)
    : data_(std::move(event)) {}

GateableEvent::GateableEvent(PageActionEvent event) : data_(std::move(event)) {}

GateableEvent::GateableEvent(const GateableEvent&) = default;
GateableEvent& GateableEvent::operator=(const GateableEvent&) = default;
GateableEvent::GateableEvent(GateableEvent&&) = default;
GateableEvent& GateableEvent::operator=(GateableEvent&&) = default;
GateableEvent::~GateableEvent() = default;

GateableEvent::Type GateableEvent::type() const {
  return static_cast<Type>(data_.index());
}

const GURL& GateableEvent::destination() const {
  return std::visit(
      [](const auto& event) -> const GURL& { return event.destination; },
      data_);
}

const GURL* GateableEvent::source() const {
  return std::visit(
      absl::Overload{
          [](const NavigationRequestEvent& event) -> const GURL* {
            return &event.source;
          },
          [](const NavigationResponseEvent& event) -> const GURL* {
            return &event.source;
          },
          [](const PageActionEvent&) -> const GURL* { return nullptr; },
      },
      data_);
}

std::string GateableEventTypeToString(GateableEvent::Type type) {
  switch (type) {
    case GateableEvent::Type::kNavigationRequest:
      return "NavigationRequest";
    case GateableEvent::Type::kNavigationResponse:
      return "NavigationResponse";
    case GateableEvent::Type::kPageAction:
      return "PageAction";
  }
}

}  // namespace origin_gating
