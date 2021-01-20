// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/policy_export.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace policy {

class CloudPolicyStore;

// Downloads, verifies, caches and retrieves external data referenced by
// policies.
// This a common base class used by cloud policy implementations and mocks.
class POLICY_EXPORT CloudExternalDataManager : public ExternalDataManager {
 public:
  struct POLICY_EXPORT MetadataEntry {
    MetadataEntry();
    MetadataEntry(const std::string& url, const std::string& hash);

    bool operator!=(const MetadataEntry& other) const;

    std::string url;
    std::string hash;
  };
  // Maps from policy names to the metadata specifying the external data that
  // each of the policies references.
  typedef std::map<std::string, MetadataEntry> Metadata;

  CloudExternalDataManager();
  virtual ~CloudExternalDataManager();

  // Sets the source of external data references to |policy_store|. The manager
  // will start observing |policy_store| so that when external data references
  // change, obsolete data can be deleted and new data can be downloaded. If the
  // |policy_store| is destroyed before the manager, the connection must be
  // severed first by calling SetPolicyStore(NULL).
  virtual void SetPolicyStore(CloudPolicyStore* policy_store);

  // Called by the |policy_store_| when policy changes.
  virtual void OnPolicyStoreLoaded() = 0;

  // Allows the manager to download external data by constructing URLLoaders
  // from |url_loader_factory|.
  virtual void Connect(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) = 0;

  // Prevents further external data downloads and aborts any downloads currently
  // in progress.
  virtual void Disconnect() = 0;

 protected:
  CloudPolicyStore* policy_store_;  // Not owned.

  base::WeakPtrFactory<CloudExternalDataManager> weak_factory_{this};

 private:
  DISALLOW_COPY_AND_ASSIGN(CloudExternalDataManager);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_CLOUD_EXTERNAL_DATA_MANAGER_H_
