// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CONTEXT_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CONTEXT_H_

#include "base/functional/callback_forward.h"

namespace ash::secure_channel {

// An interface used to decode and encode messages.
class SecureContext {
 public:
  typedef base::OnceCallback<void(const std::string& message)>
      EncodeMessageCallback;
  typedef base::RepeatingCallback<void(const std::string& message)>
      DecodeMessageCallback;

  // The protocol version used during authentication.
  enum ProtocolVersion {
    PROTOCOL_VERSION_THREE_ZERO,  // 3.0
    PROTOCOL_VERSION_THREE_ONE,   // 3.1
  };

  virtual ~SecureContext() {}

  // Decodes the |encoded_message| and returns the result if the message is not
  // out of order. If it is an out of order messagge, it will be queued and
  // handled after all the messages are in order. This function is asynchronous
  // because the ChromeOS implementation requires a DBus call.
  virtual void DecodeAndDequeue(const std::string& encoded_message,
                                DecodeMessageCallback callback) = 0;

  // Encodes the |message| and returns the result.
  // This function is asynchronous because the ChromeOS implementation requires
  // a DBus call.
  virtual void Encode(const std::string& message,
                      EncodeMessageCallback callback) = 0;

  // Returns the protocol version that was used during authentication.
  virtual ProtocolVersion GetProtocolVersion() const = 0;

  // Returns data specific to the current session that can be used in
  // cryptographic operations to bind to the channel.
  virtual std::string GetChannelBindingData() const = 0;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_SECURE_CONTEXT_H_
