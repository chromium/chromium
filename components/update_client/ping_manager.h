// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
#define COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

namespace update_client {

class Configurator;
class Component;

class PingManager : public base::RefCountedThreadSafe<PingManager> {
 public:
  explicit PingManager(scoped_refptr<Configurator> config);

  PingManager(const PingManager&) = delete;
  PingManager& operator=(const PingManager&) = delete;

  // Sends a ping for the |item|. |callback| is invoked after the ping is sent
  // or an error has occured. The ping itself is not persisted and it will
  // be discarded if it has not been sent for any reason.
  // |error| is 0 if the ping was sent successfully, otherwise |error| contains
  // a value with no particular meaning for the caller.
  virtual void SendPing(const Component& component, base::OnceClosure callback);

 protected:
  virtual ~PingManager();

 private:
  friend class base::RefCountedThreadSafe<PingManager>;

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<Configurator> config_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_PING_MANAGER_H_
