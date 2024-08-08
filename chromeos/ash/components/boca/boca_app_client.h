// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash {

// Defines the interface for sub features to access hub Events
class BocaAppClient {
 public:
  // Interface for observing events.
  class Observer : public base::CheckedObserver {
   public:
    // Notifies when session started. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionStarted(const std::string& session_id) = 0;

    // Notifies when session ended. Pure virtual function, must be handled by
    // observer.
    virtual void OnSessionEnded(const std::string& session_id) = 0;

    // Notifies when bundle updated. In the event of session started with a
    // bundle configured, both events will be fired.
    virtual void OnBundleUpdated(const ::boca::Bundle& bundle);

    // Notifies when caption producer's config updated.
    virtual void OnProducerCaptionConfigUpdated(
        const ::boca::CaptionsConfig& config);

    // Notifies when caption consumer's config updated.
    virtual void OnConsumerCaptionConfigUpdated(
        const ::boca::CaptionsConfig& config);
  };

  BocaAppClient(const BocaAppClient&) = delete;
  BocaAppClient& operator=(const BocaAppClient&) = delete;

  static BocaAppClient* Get();

  // Returns the IdentityManager for the active user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the URLLoaderFactory associated with user profile.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  BocaAppClient();
  virtual ~BocaAppClient();

 private:
  base::ObserverList<Observer> observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BOCA_APP_CLIENT_H_
