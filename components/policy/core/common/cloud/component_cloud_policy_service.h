// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/component_cloud_policy_service_observer.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema_registry.h"
#include "components/policy/core/common/values_util.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

class ResourceCache;
class SchemaMap;

// Manages cloud policy for components (currently used for device local accounts
// and policy for extensions --> go/cros-ent-p4ext-dd).
//
// This class takes care of fetching, validating, storing and updating policy
// for components.
//
// Note that the policies for all components, as returned by the server, are
// downloaded and cached, regardless of the current state of the schema
// registry. However, exposed are only the policies whose components are present
// in the schema registry.
//
// The exposed policies are guaranteed to be conformant to the corresponding
// schemas. Values that do not pass validation against the schema are dropped.
class POLICY_EXPORT ComponentCloudPolicyService
    : public CloudPolicyClient::Observer,
      public CloudPolicyCore::Observer,
      public CloudPolicyStore::Observer,
      public SchemaRegistry::Observer {
 public:
  class POLICY_EXPORT Delegate {
   public:
    virtual ~Delegate();

    // Invoked whenever the policy served by policy() changes. This is also
    // invoked for the first time once the backend is initialized, and
    // is_initialized() becomes true.
    virtual void OnComponentCloudPolicyUpdated() = 0;
  };

  // |policy_type| specifies the policy type that should be fetched. The only
  // allowed values are: |dm_protocol::kChromeExtensionPolicyType|,
  // |dm_protocol::kChromeSigninExtensionPolicyType|.
  //
  // The |delegate| is notified of updates to the downloaded policies and must
  // outlive this object.
  //
  // |schema_registry| is used to filter the fetched policies against the list
  // of installed extensions and to validate the policies against corresponding
  // schemas. It must outlive this object.
  //
  // |core| is used to obtain the CloudPolicyStore and CloudPolicyClient used
  // by this service. The store will be the source of the registration status
  // and registration credentials; the client will be used to fetch cloud
  // policy. It must outlive this object.
  //
  // The |core| MUST not be connected yet when this service is created;
  // |client| must be the client that will be connected to the |core|. This
  // is important to make sure that this service appends any necessary policy
  // fetch types to the |client| before the |core| gets connected and before
  // the initial policy fetch request is sent out.
  //
  // |cache| is used to load and store local copies of the downloaded policies.
  //
  // Download scheduling, validation and caching of policies are done via the
  // |backend_task_runner|, which must support file I/O.
  ComponentCloudPolicyService(
      const std::string& policy_type,
      Delegate* delegate,
      SchemaRegistry* schema_registry,
      CloudPolicyCore* core,
      CloudPolicyClient* client,
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      std::unique_ptr<ResourceCache> cache,
#endif
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ComponentCloudPolicyService(const ComponentCloudPolicyService&) = delete;
  ComponentCloudPolicyService& operator=(const ComponentCloudPolicyService&) =
      delete;
  ~ComponentCloudPolicyService() override;

  // Returns true if |domain| is supported by the service.
  static bool SupportsDomain(PolicyDomain domain);

  // Returns true if the backend is initialized, and the initial policies are
  // being served.
  bool is_initialized() const { return policy_installed_; }

  // Returns the current policies for components.
  const PolicyBundle& policy() const { return policy_; }

  const ComponentPolicyMap& component_policy_map() const {
    return component_policy_map_;
  }

  // Add/Remove observer to notify about component policy changes. AddObserver
  // triggers an OnComponentPolicyUpdated notification to be posted to the newly
  // added observer.
  void AddObserver(ComponentCloudPolicyServiceObserver* observer);
  void RemoveObserver(ComponentCloudPolicyServiceObserver* observer);

  // Deletes all the cached component policy.
  void ClearCache();

  // SchemaRegistry::Observer implementation:
  void OnSchemaRegistryReady() override;
  void OnSchemaRegistryUpdated(bool has_new_schemas) override;

  // CloudPolicyCore::Observer implementation:
  void OnCoreConnected(CloudPolicyCore* core) override;
  void OnCoreDisconnecting(CloudPolicyCore* core) override;
  void OnRefreshSchedulerStarted(CloudPolicyCore* core) override;

  // CloudPolicyStore::Observer implementation:
  void OnStoreLoaded(CloudPolicyStore* store) override;
  void OnStoreError(CloudPolicyStore* store) override;

  // CloudPolicyClient::Observer implementation:
  void OnPolicyFetched(CloudPolicyClient* client) override;
  void OnRegistrationStateChanged(CloudPolicyClient* client) override;
  void OnClientError(CloudPolicyClient* client) override;

 private:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  class Backend;

  void UpdateFromSuperiorStore();
  void UpdateFromClient();
  void UpdateFromSchemaRegistry();
  void Disconnect();
  void SetPolicy(std::unique_ptr<PolicyBundle> policy);
  void FilterAndInstallPolicy();
  void NotifyComponentPolicyUpdated();

  std::string policy_type_;
  raw_ptr<Delegate> delegate_;
  raw_ptr<SchemaRegistry> schema_registry_;
  raw_ptr<CloudPolicyCore> core_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // The |backend_| handles all download scheduling, validation and caching of
  // policies. It is instantiated on the thread |this| runs on but after that,
  // must only be accessed and eventually destroyed via the
  // |backend_task_runner_|.
  std::unique_ptr<Backend> backend_;

  // The currently registered components for each policy domain. Used for
  // filtering and validation of the component policies.
  scoped_refptr<SchemaMap> current_schema_map_;
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  // Contains all the policies loaded from the store, before having been
  // filtered and validated by the |current_schema_map_|.
  std::unique_ptr<PolicyBundle> unfiltered_policy_;

  // Contains the same policies as |unfiltered_policy_|, but in JSON format.
  ComponentPolicyMap component_policy_map_;

  // Contains all the current policies for components, filtered and validated by
  // the |current_schema_map_|.
  PolicyBundle policy_;

  // Whether policies are being served.
  bool policy_installed_ = false;

  base::ObserverList<ComponentCloudPolicyServiceObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Must be the last member.
  base::WeakPtrFactory<ComponentCloudPolicyService> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_COMPONENT_CLOUD_POLICY_SERVICE_H_
