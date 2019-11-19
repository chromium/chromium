// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_H_
#define CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_H_

#include <ostream>
#include <string>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "chromeos/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace chromeos {

namespace secure_channel {

// Parameters associated with a client request, which should be tightly-coupled
// to the associated communication channel.
class ClientConnectionParameters {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnConnectionRequestCanceled() = 0;
  };

  explicit ClientConnectionParameters(const std::string& feature);
  virtual ~ClientConnectionParameters();

  const base::UnguessableToken& id() const { return id_; }
  const std::string& feature() const { return feature_; }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns whether the client is waiting for a response. In this context, this
  // means that the client has not canceled the request by disconnecting its
  // ConnectionDelegate binding and also has not yet had either of its delegate
  // callbacks invoked.
  bool IsClientWaitingForResponse();

  // Alerts the client that the connection attempt has failed due to |reason|.
  // This function can only be called if IsActive() is true and
  // SetConnectionSucceeded() has not been invoked.
  void SetConnectionAttemptFailed(mojom::ConnectionAttemptFailureReason reason);

  // Alerts the client that the connection has succeeded, providing the client
  // with a Channel and a receiver to bind a MessageReceiver. This function can
  // only be called if IsActive() is true and SetConnectionAttemptFailed() has
  // not been invoked.
  void SetConnectionSucceeded(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver> message_receiver_receiver);

  bool operator==(const ClientConnectionParameters& other) const;
  bool operator<(const ClientConnectionParameters& other) const;

 protected:
  virtual bool HasClientCanceledRequest() = 0;
  virtual void PerformSetConnectionAttemptFailed(
      mojom::ConnectionAttemptFailureReason reason) = 0;
  virtual void PerformSetConnectionSucceeded(
      mojo::PendingRemote<mojom::Channel> channel,
      mojo::PendingReceiver<mojom::MessageReceiver>
          message_receiver_receiver) = 0;

  void NotifyConnectionRequestCanceled();

 private:
  void VerifyDelegateWaitingForResponse(const std::string& function_name);

  std::string feature_;
  base::UnguessableToken id_;

  base::ObserverList<Observer>::Unchecked observer_list_;

  bool has_invoked_delegate_function_ = false;

  DISALLOW_COPY_AND_ASSIGN(ClientConnectionParameters);
};

std::ostream& operator<<(std::ostream& stream,
                         const ClientConnectionParameters& details);

}  // namespace secure_channel

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_SECURE_CHANNEL_CLIENT_CONNECTION_PARAMETERS_H_
