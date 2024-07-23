// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_H_
#define COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_H_

#include <memory>
#include <string>

#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "components/invalidation/public/invalidation.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace invalidation {

// For the migration from topic based (Fandango) invalidations to direct
// message invalidations, this is a thin wrapper around the topic based
// invalidations. The only reason for having this is to provide the `type`
// method in the interface (as opposed to using the `topic` method).
// TODO(b/350013667) Once we fully migrated to direct message
// invalidations, we can delete `invalidation::Invalidation` and rename this to
// Invalidation.
class DirectInvalidation : public invalidation::Invalidation {
 public:
  using invalidation::Invalidation::Invalidation;

  std::string type() const;
  base::Time issue_timestamp() const;
};

// Interface to handle obtained registration tokens.
class RegistrationTokenHandler {
 public:
  virtual ~RegistrationTokenHandler() = default;

  // Will be called whenever the registration token was obtained or refreshed.
  virtual void OnRegistrationTokenReceived(
      const std::string& registration_token,
      base::Time token_end_of_life) = 0;
};

// Represents invalidations availability status.
enum class InvalidationsExpected {
  kYes,   // Both registration and upload succeeded.
  kMaybe  // One of registration or upload failed. Invalidations might
          // still be received, but we should not rely on it.
};

// The `InvalidationListener` is receiving invalidation data messages via FM
// (formerly known as FCM formerly known as GCM). It also obtains the FM app
// registration token, aka app instance id.
//
// Expected elements of FM message data:
// {
//   "type": string, # used to route the message to the correct observer.
//   "version" : int,
//   "payload" : string
// }
//
// Note that invalidation messages might get dropped while the service is not
// listening.
class InvalidationListener {
 public:
  // Application id for the `GCMDriver` used by invalidations.
  static constexpr char kFmAppId[] = "com.google.chrome.fcm.invalidations";
  // The pantheon project number was decided by the serverside team.
  // The number is used to deliver invalidations with FCM.
  static constexpr char kProjectNumberEnterprise[] = "1013309121859";

  // Represents version of the format of the invalidation messages that is
  // parsed by the listener.
  static constexpr int kInvalidationProtocolVersion = 1;

  virtual ~InvalidationListener() = default;

  enum class RegistrationTokenUploadStatus { kSucceeded, kFailed };

  class Observer : public base::CheckedObserver {
   public:
    // Called when expectations about invalidations changed in the listener.
    virtual void OnExpectationChanged(InvalidationsExpected expected) = 0;

    // Called when an invalidation has been received.
    virtual void OnInvalidationReceived(
        const DirectInvalidation& invalidation) = 0;

    // Will be called when the Observer is added the `InvalidationListener`.
    // This way, the `InvalidationListener` will know which subject the
    // `Observer` is interested in.
    virtual std::string GetType() const = 0;
  };

  virtual void AddObserver(Observer* handler) = 0;
  virtual bool HasObserver(const Observer* handler) const = 0;
  virtual void RemoveObserver(const Observer* handler) = 0;

  // Creates an `InvalidationListener` for `app_id_`.
  // `gcm_driver` will typically be obtained like this:
  //   - device: g_browser_process->gcm_driver();
  //   - profile: instance_id::InstanceIDProfileServiceFactory
  //                         ::GetForProfile(profile)->driver());
  // `instance_id_driver` is typically obtained like this:
  //   - device: constructed on demand from `gcm_driver`;
  //   - profile: gcm::GCMProfileServiceFactory
  //                 ::GetForProfile(profile)->driver();
  //
  // `project_number` is a pantheon project number, e.g.
  //   - `1013309121859` for DMServer invalidations, see the comment for
  // `kProjectNumberEnterprise`.
  //
  // `log_prefix` is a string that will be added in the beginning of each
  // emitted log. The string should be wrapped with square brackets, e.g.
  // `log_prefix = "[prefix]"`.
  static std::unique_ptr<InvalidationListener> Create(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      std::string project_number,
      std::string log_prefix);

  // The following functions are to be used by the `RegistrationTokenHandler`
  // only.
  // This must be called at most once before shutdown.
  virtual void Start(RegistrationTokenHandler* handler) = 0;
  virtual void SetRegistrationUploadStatus(
      RegistrationTokenUploadStatus status) = 0;
  virtual void Shutdown() = 0;
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_INVALIDATION_LISTENER_H_
