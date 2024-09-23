// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

// A socket-like wrapper around Nearby Connections that allows for asynchronous
// reads and writes.
class NearbyConnection {
 public:
  using ReadCallback =
      base::OnceCallback<void(std::optional<std::vector<uint8_t>> bytes)>;

  NearbyConnection();
  NearbyConnection(const NearbyConnection&) = delete;
  NearbyConnection& operator=(const NearbyConnection&) = delete;
  virtual ~NearbyConnection();

  // Reads a stream of bytes from the remote device. Invoke |callback| when
  // there is incoming data or when the socket is closed. Previously set
  // callback will be replaced by |callback|. Must not be used on a already
  // closed connection.
  virtual void Read(ReadCallback callback) = 0;

  // Writes an outgoing stream of bytes to the remote device asynchronously.
  // Must not be used on a already closed connection.
  virtual void Write(std::vector<uint8_t> bytes) = 0;

  // Closes the socket and disconnects from the remote device. This object will
  // be invalidated after |callback| in SetDisconnectionListener is invoked.
  virtual void Close() = 0;

  // Listens to the socket being closed. Invoke |callback| when the socket is
  // closed. This object will be invalidated after |listener| is invoked.
  // Previously set listener will be replaced by |listener|.
  virtual void SetDisconnectionListener(base::OnceClosure listener) = 0;

  base::WeakPtr<NearbyConnection> GetWeakPtr();

 private:
  base::WeakPtrFactory<NearbyConnection> weak_ptr_factory_{this};
};

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_COMMON_CONNECTIONS_MANAGER_NEARBY_CONNECTION_H_
