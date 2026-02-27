// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_AI_COMMON_PRIVATE_AI_LOGGER_H_
#define COMPONENTS_PRIVATE_AI_COMMON_PRIVATE_AI_LOGGER_H_

#include <string_view>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace base {
class Location;
}

namespace private_ai {

// Handles logging in PrivateAI and notifies observers.
class PrivateAiLogger {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnLogInfo(const base::Location& location,
                           std::string_view message) {}
    virtual void OnLogError(const base::Location& location,
                            std::string_view message) {}
  };

  PrivateAiLogger();
  ~PrivateAiLogger();

  PrivateAiLogger(const PrivateAiLogger&) = delete;
  PrivateAiLogger& operator=(const PrivateAiLogger&) = delete;

  void LogInfo(const base::Location& location, std::string_view message);
  void LogError(const base::Location& location, std::string_view message);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace private_ai

#endif  // COMPONENTS_PRIVATE_AI_COMMON_PRIVATE_AI_LOGGER_H_
