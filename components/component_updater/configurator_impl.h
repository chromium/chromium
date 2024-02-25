// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_
#define COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/configurator.h"
#include "url/gurl.h"

namespace base {
class Version;
}

namespace update_client {
class CommandLineConfigPolicy;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace component_updater {

// Helper class for the implementations of update_client::Configurator.
// Can be used both on iOS and other platforms.
class ConfiguratorImpl {
 public:
  ConfiguratorImpl(const update_client::CommandLineConfigPolicy& config_policy,
                   bool require_encryption);
  ConfiguratorImpl(const ConfiguratorImpl&) = delete;
  ConfiguratorImpl& operator=(const ConfiguratorImpl&) = delete;
  ~ConfiguratorImpl();

  // Delay from calling Start() to the first update check.
  base::TimeDelta InitialDelay() const;

  // Delay to every subsequent update check. 0 means don't check.
  base::TimeDelta NextCheckDelay() const;

  // Minimum delta time before an on-demand check is allowed for the same
  // component.
  base::TimeDelta OnDemandDelay() const;

  // The time delay between applying updates for different components.
  base::TimeDelta UpdateDelay() const;

  // The URLs for the update checks. The URLs are tried in order, the first one
  // that succeeds wins.
  std::vector<GURL> UpdateUrl() const;

  // The URLs for pings. Returns an empty vector if and only if pings are
  // disabled. Similarly, these URLs have a fall back behavior too.
  std::vector<GURL> PingUrl() const;

  // Version of the application. Used to compare the component manifests.
  const base::Version& GetBrowserVersion() const;

  // Returns the OS's long name like "Windows", "Mac OS X", etc.
  std::string GetOSLongName() const;

  // Parameters added to each url request. It can be empty if none are needed.
  // Returns a map of name-value pairs that match ^[-_a-zA-Z0-9]$ regex.
  base::flat_map<std::string, std::string> ExtraRequestParams() const;

  // Provides a hint for the server to control the order in which multiple
  // download urls are returned.
  std::string GetDownloadPreference() const;

  // True means that this client can handle delta updates.
  bool EnabledDeltas() const;

  // True is the component updates are enabled.
  bool EnabledComponentUpdates() const;

  // True means that the background downloader can be used for downloading
  // non on-demand components.
  bool EnabledBackgroundDownloader() const;

  // True if signing of update checks is enabled.
  bool EnabledCupSigning() const;

  // Returns the app GUID with which Chrome is registered with Google Update, or
  // an empty string if this brand does not integrate with Google Update.
  std::string GetAppGuid() const;

  // Returns the class factory to create protocol parser and protocol
  // serializer object instances.
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const;

  std::optional<bool> IsMachineExternallyManaged() const;

  update_client::UpdaterStateProvider GetUpdaterStateProvider() const;

  bool IsConnectionMetered() const;

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, std::string> extra_info_;
  const bool background_downloads_enabled_;
  const bool deltas_enabled_;
  const bool fast_update_;
  const bool pings_enabled_;
  const bool require_encryption_;
  const GURL url_source_override_;
  const base::TimeDelta initial_delay_;
};

}  // namespace component_updater

#endif  // COMPONENTS_COMPONENT_UPDATER_CONFIGURATOR_IMPL_H_
