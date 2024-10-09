// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_
#define COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/version.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/seed_response.h"
#include "components/version_info/channel.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace network_time {
class NetworkTimeTracker;
}

class PrefService;

namespace variations {

// An abstraction of operations that depend on the embedder's (e.g. Chrome)
// environment.
class VariationsServiceClient {
 public:
  virtual ~VariationsServiceClient() = default;

  // Returns the version to use for variations seed simulation.
  virtual base::Version GetVersionForSimulation() = 0;

  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;
  virtual network_time::NetworkTimeTracker* GetNetworkTimeTracker() = 0;

  // Returns whether the embedder overrides the value of the restrict parameter.
  // |parameter| is an out-param that will contain the value of the restrict
  // parameter if true is returned.
  virtual bool OverridesRestrictParameter(std::string* parameter) = 0;

  // Gets the channel to use for variations. The --fake-variations-channel
  // override switch takes precedence over the embedder-provided channel.
  // If that switch is not set, it will return the embedder-provided channel,
  // (which could be UNKNOWN).
  version_info::Channel GetChannelForVariations();

  // Returns the current form factor of the device.
  virtual Study::FormFactor GetCurrentFormFactor();

  // Returns the directory in which to store variations seed files.
  virtual base::FilePath GetVariationsSeedFileDir();

  // If a native variations service that directly fetches the seed from the
  // server is implemented, returns the SeedResponse from the native variations
  // seed store, and removes the seed from the native storage given that we can
  // assume that the returned seed would be stored into Chrome Prefs. Otherwise,
  // returns nullptr.
  virtual std::unique_ptr<SeedResponse> TakeSeedFromNativeVariationsSeedStore();

  // Returns whether the client is enterprise.
  // TODO(manukh): crbug.com/1003025. This is inconsistent with UMA which
  // analyzes brand_code to determine if the client is an enterprise user:
  // - For android, linux, and iOS, they are consistent because both UMA and
  //   chromium consider all such devices as non-enterprise.
  // - For mac and chromeOS, they are inconsistent because UMA does not consider
  //   any such devices as enterprise, but chromium does.
  // - For windows, both consider some clients as enterprise, but use different
  //   chromium doesn't use brand_code so they may have inconsistent results.
  // That being said, studies restricted by finch won't need to filter on UMA as
  // well. But this could be confusing and could prevent using UMA filters on a
  // non finch-filtered study to analyze the finch-filtered launch potential.
  virtual bool IsEnterprise() = 0;

  // Removes stored Google Groups variations information for deleted profiles.
  // Must be called at startup, prior to the variations Google Groups being
  // read.
  // This is a no-op on platforms that do not support multiple profiles.
  virtual void RemoveGoogleGroupsFromPrefsForDeletedProfiles(
      PrefService* local_state) = 0;

 private:
  // Gets the channel of the embedder. But all variations callers should use
  // |GetChannelForVariations()| instead.
  virtual version_info::Channel GetChannel() = 0;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_SERVICE_VARIATIONS_SERVICE_CLIENT_H_
