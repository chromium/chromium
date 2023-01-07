// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EVENT_HANDLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EVENT_HANDLER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {

// Receives incoming events and notifies a list of observers.
//
// - It is safe to add/remove observers at any time.
// - Observers are allowed to fire new events, take care to avoid infinite
// loops!
// - This class is NOT thread-safe!
class EventHandler {
 public:
  // The unique event key consists of the event type and the event source ID.
  using EventKey = std::pair<EventProto::KindCase, std::string>;

  // Interface for observers of the event handler.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnEvent(const EventKey& key) = 0;
  };

  EventHandler();

  EventHandler(const EventHandler&) = delete;
  EventHandler& operator=(const EventHandler&) = delete;

  ~EventHandler();

  void DispatchEvent(const EventKey& key);

  static absl::optional<EventKey> CreateEventKeyFromProto(
      const EventProto& proto);

  void AddObserver(Observer* observer);
  void RemoveObserver(const Observer* observer);

 private:
  base::ReentrantObserverList<Observer> observers_{
      base::ObserverListPolicy::EXISTING_ONLY};
};

// Intended for debugging.
std::ostream& operator<<(std::ostream& out,
                         const EventProto::KindCase& event_case);
std::ostream& operator<<(std::ostream& out, const EventHandler::EventKey& key);

}  //  namespace autofill_assistant

#endif  //  COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_EVENT_HANDLER_H_
