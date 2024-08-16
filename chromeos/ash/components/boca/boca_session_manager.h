// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_

#include <string>
#include <vector>

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace boca {
class UserIdentity;
class Bundle;
class CaptionsConfig;
}  // namespace boca

namespace ash::boca {

class BocaSessionManager {
 public:
  enum class BocaAction {
    kDefault = 0,
    kOntask = 1,
    kLiveCaption = 2,
    kTranslation = 3,
    kTranscription = 4,
  };

  enum ErrorLevel {
    kInfo = 0,
    kWarn = 1,
    kFatal = 2,
  };

  struct BocaError {
    BocaError(BocaAction action_param,
              ErrorLevel error_level_param,
              std::string error_message_param)
        : action(action_param),
          error_level(error_level_param),
          error_message(error_message_param) {}
    const BocaAction action;
    const ErrorLevel error_level;
    const std::string error_message;
  };

  BocaSessionManager();
  BocaSessionManager(const BocaSessionManager&) = delete;
  BocaSessionManager& operator=(const BocaSessionManager&) = delete;
  ~BocaSessionManager();

  // Interface for observing events.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when session started. Pure virtual function, must be handled by
    // observer. Session metadata will be provided when fired.
    virtual void OnSessionStarted(const std::string& session_id,
                                  const ::boca::UserIdentity& producer) = 0;

    // Notifies when session ended. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionEnded(const std::string& session_id) = 0;

    // Notifies when bundle updated. In the event of session started with a
    // bundle configured, both events will be fired.
    virtual void OnBundleUpdated(const ::boca::Bundle& bundle);

    // Notifies when session config updated for specific group.
    virtual void OnSessionCaptionConfigUpdated(
        const std::string& group_name,
        const ::boca::CaptionsConfig& config);

    // Notifies when local caption config updated.
    virtual void OnLocalCaptionConfigUpdated(
        const ::boca::CaptionsConfig& config);

    // Notifies when session roster updated.
    virtual void OnSessionRosterUpdated(
        const std::string& group_name,
        const std::vector<::boca::UserIdentity>& consumers);
  };

  void NotifyError(BocaError error);
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  base::ObserverList<Observer> observers_;
};
}  // namespace ash::boca
#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_SESSION_MANAGER_H_
