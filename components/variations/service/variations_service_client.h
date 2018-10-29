// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "base/version.h"
#include "components/version_info/version_info.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace network_time {
class NetworkTimeTracker;
}

namespace variations {

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class VariationsServiceClient {
 public:
  virtual ~VariationsServiceClient() {}

  // Returns a callback that when run returns the base::Version to use for
  // variations seed simulation. VariationsService guarantees that the callback
  // will be run on a background thread that permits blocking.
  virtual base::Callback<base::Version(void)>
  GetVersionForSimulationCallback() = 0;

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;
  virtual network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;

  // Gets the channel of the embedder.
  virtual version_info::Channel GetChannel() = 0;

  // Gets whether this platform supports experiments which retain their group
  // assignments across runs.
  // TODO(paulmiller): Remove this once https://crbug.com/866722 is resolved.
  virtual bool GetSupportsPermanentConsistency();

  // Returns whether the embedder overrides the value of the restrict parameter.
  // |parameter| is an out-param that will contain the value of the restrict
  // parameter if true is returned.
  virtual bool OverridesRestrictParameter(std::string* parameter) = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_
