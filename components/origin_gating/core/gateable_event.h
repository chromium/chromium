// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ORIGIN_GATING_CORE_GATEABLE_EVENT_H_
#define COMPONENTS_ORIGIN_GATING_CORE_GATEABLE_EVENT_H_

#include <string>
#include <variant>

#include "base/containers/enum_set.h"
#include "components/origin_gating/core/client_tool.h"
#include "url/gurl.h"

namespace origin_gating {

struct NavigationRequestEvent {
  friend bool operator==(const NavigationRequestEvent&,
                         const NavigationRequestEvent&) = default;

  GURL source;
  GURL destination;
};

struct NavigationResponseEvent {
  friend bool operator==(const NavigationResponseEvent&,
                         const NavigationResponseEvent&) = default;

  GURL source;
  GURL destination;
};

struct PageActionEvent {
  friend bool operator==(const PageActionEvent&,
                         const PageActionEvent&) = default;

  GURL destination;
  ClientTool tool;
};

// The event being evaluated by origin gating. A single predicate
// may apply to some events but not others (e.g. dangerous MIME type check is
// applicable only to navigation response), so callers pass the relevant event
// to ComputeGatingDecision and each predicate declares the set of events it
// applies to.
class GateableEvent {
 public:
  enum class Type {
    // A navigation request is starting, or is being redirected.
    kNavigationRequest,
    // A navigation response is being processed (the final, committed URL).
    kNavigationResponse,
    // An action is being performed on an existing tab/page.
    kPageAction,
  };

  using enum Type;

  explicit GateableEvent(NavigationRequestEvent event);
  explicit GateableEvent(NavigationResponseEvent event);
  explicit GateableEvent(PageActionEvent event);

  GateableEvent(const GateableEvent&);
  GateableEvent& operator=(const GateableEvent&);
  GateableEvent(GateableEvent&&);
  GateableEvent& operator=(GateableEvent&&);
  ~GateableEvent();

  friend bool operator==(const GateableEvent&, const GateableEvent&) = default;

  Type type() const;

  bool is_navigation_request() const {
    return type() == Type::kNavigationRequest;
  }
  bool is_navigation_response() const {
    return type() == Type::kNavigationResponse;
  }
  bool is_page_action() const { return type() == Type::kPageAction; }

  // Returns the target/destination URL common to all events.
  const GURL& destination() const;

  // Returns the navigation source URL for `NavigationRequestEvent` and
  // `NavigationResponseEvent`, or `nullptr` for `PageActionEvent`.
  const GURL* source() const;

  const NavigationRequestEvent* GetIfNavigationRequest() const {
    return std::get_if<NavigationRequestEvent>(&data_);
  }
  const NavigationResponseEvent* GetIfNavigationResponse() const {
    return std::get_if<NavigationResponseEvent>(&data_);
  }
  const PageActionEvent* GetIfPageAction() const {
    return std::get_if<PageActionEvent>(&data_);
  }

 private:
  // Order of alternatives must match `Type` enumerators.
  std::variant<NavigationRequestEvent, NavigationResponseEvent, PageActionEvent>
      data_;
};

std::string GateableEventTypeToString(GateableEvent::Type type);

using GateableEventSet = base::EnumSet<GateableEvent::Type,
                                       GateableEvent::kNavigationRequest,
                                       GateableEvent::kPageAction>;

}  // namespace origin_gating

#endif  // COMPONENTS_ORIGIN_GATING_CORE_GATEABLE_EVENT_H_
