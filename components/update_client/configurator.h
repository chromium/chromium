// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

class GURL;
class PrefService;

namespace base {
class Version;
}

namespace update_client {

class CrxDownloaderFactory;
class NetworkFetcherFactory;
class PatcherFactory;
class PersistedData;
class ProtocolHandlerFactory;
class UnzipperFactory;

using UpdaterStateAttributes = base::flat_map<std::string, std::string>;
using UpdaterStateProvider =
    base::RepeatingCallback<UpdaterStateAttributes(bool is_machine)>;

// Controls the component updater behavior.
class Configurator : public base::RefCountedThreadSafe<Configurator> {
 public:
  // Delay from calling Start() to the first update check.
  virtual base::TimeDelta InitialDelay() const = 0;

  // Delay to every subsequent update check. 0 means don't check.
  virtual base::TimeDelta NextCheckDelay() const = 0;

  // Minimum delta time before an on-demand check is allowed
  // for the same component.
  virtual base::TimeDelta OnDemandDelay() const = 0;

  // The time delay between applying updates for different
  // components.
  virtual base::TimeDelta UpdateDelay() const = 0;

  // The URLs for the update checks. The URLs are tried in order, the first one
  // that succeeds wins. Since some components cannot be updated over HTTP,
  // HTTPS URLs should appear first.
  virtual std::vector<GURL> UpdateUrl() const = 0;

  // The URLs for pings. Returns an empty vector if and only if pings are
  // disabled. Similarly, these URLs have a fall back behavior too.
  virtual std::vector<GURL> PingUrl() const = 0;

  // The ProdId is used as a prefix in some of the version strings which appear
  // in the protocol requests. Possible values include "chrome", "chromecrx",
  // "chromiumcrx", and "unknown".
  virtual std::string GetProdId() const = 0;

  // Version of the application. Used to compare the component manifests.
  virtual base::Version GetBrowserVersion() const = 0;

  // Returns the value we use for the "updaterchannel=" and "prodchannel="
  // parameters. Possible return values include: "canary", "dev", "beta", and
  // "stable".
  virtual std::string GetChannel() const = 0;

  // Returns the language for the present locale. Possible return values are
  // standard tags for languages, such as "en", "en-US", "de", "fr", "af", etc.
  virtual std::string GetLang() const = 0;

  // Returns the OS's long name like "Windows", "Mac OS X", etc.
  virtual std::string GetOSLongName() const = 0;

  // Parameters added to each url request. It can be empty if none are needed.
  // Returns a map of name-value pairs that match ^[-_a-zA-Z0-9]$ regex.
  virtual base::flat_map<std::string, std::string> ExtraRequestParams()
      const = 0;

  // Provides a hint for the server to control the order in which multiple
  // download urls are returned. The hint may or may not be honored in the
  // response returned by the server.
  // Returns an empty string if no policy is in effect.
  virtual std::string GetDownloadPreference() const = 0;

  virtual scoped_refptr<NetworkFetcherFactory> GetNetworkFetcherFactory() = 0;

  virtual scoped_refptr<CrxDownloaderFactory> GetCrxDownloaderFactory() = 0;

  virtual scoped_refptr<UnzipperFactory> GetUnzipperFactory() = 0;

  virtual scoped_refptr<PatcherFactory> GetPatcherFactory() = 0;

  // True means that this client can handle delta updates.
  virtual bool EnabledDeltas() const = 0;

  // True means that the background downloader can be used for downloading
  // non on-demand components.
  virtual bool EnabledBackgroundDownloader() const = 0;

  // True if signing of update checks is enabled.
  virtual bool EnabledCupSigning() const = 0;

  // Returns a PrefService that the update_client can use to store persistent
  // update information. The PrefService must outlive the entire update_client,
  // and be safe to access from the sequence the update_client is constructed
  // on.
  // Returning null is safe and will disable any functionality that requires
  // persistent storage.
  virtual PrefService* GetPrefService() const = 0;

  // Returns a PersistedData instance that the update_client can use to access
  // to update information. Similar to PrefService, PersistedData must outlive
  // the entire update_client, and be safe to access from the sequence the
  // update_client is constructed on.
  virtual PersistedData* GetPersistedData() const = 0;

  // Returns true if the Chrome is installed for the current user only, or false
  // if Chrome is installed for all users on the machine. This function must be
  // called only from a blocking pool thread, as it may access the file system.
  virtual bool IsPerUserInstall() const = 0;

  // Returns the class factory to create protocol parser and protocol
  // serializer object instances.
  virtual std::unique_ptr<ProtocolHandlerFactory> GetProtocolHandlerFactory()
      const = 0;

  // Returns true if Chrome is installed on a system managed by cloud or
  // group policies, false if the system is not managed, or nullopt if the
  // platform does not support client management at all.
  virtual std::optional<bool> IsMachineExternallyManaged() const = 0;

  // Returns a callable to get the state of the platform updater, if the
  // embedder includes an updater. Returns a null callback otherwise.
  virtual UpdaterStateProvider GetUpdaterStateProvider() const = 0;

  // Returns the filepath where installed crx's should be cached for
  // puffin patches.
  virtual std::optional<base::FilePath> GetCrxCachePath() const = 0;

  virtual bool IsConnectionMetered() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<Configurator>;

  virtual ~Configurator() = default;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_CONFIGURATOR_H_
