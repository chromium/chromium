// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
#define COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_

#include "base/memory/scoped_refptr.h"
#include "components/data_sharing/public/data_sharing_service.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace data_sharing {
class DataSharingNetworkLoader;

// The internal implementation of the DataSharingService.
class DataSharingServiceImpl : public DataSharingService {
 public:
  DataSharingServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~DataSharingServiceImpl() override;

  // Disallow copy/assign.
  DataSharingServiceImpl(const DataSharingServiceImpl&) = delete;
  DataSharingServiceImpl& operator=(const DataSharingServiceImpl&) = delete;

  // DataSharingService implementation.
  bool IsEmptyService() override;
  DataSharingNetworkLoader* GetDataSharingNetworkLoader() override;
  void ReadAllGroups(
      base::OnceCallback<void(const GroupsDataSetOrFailureOutcome&)> callback)
      override;
  void ReadGroup(const std::string& group_id,
                 base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                     callback) override;
  void CreateGroup(const std::string& group_name,
                   base::OnceCallback<void(const GroupDataOrFailureOutcome&)>
                       callback) override;
  void DeleteGroup(
      const std::string& group_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void InviteMember(
      const std::string& group_id,
      const std::string& invitee_gaia_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;
  void RemoveMember(
      const std::string& group_id,
      const std::string& member_gaia_id,
      base::OnceCallback<void(PeopleGroupActionOutcome)> callback) override;

 private:
  std::unique_ptr<DataSharingNetworkLoader> data_sharing_network_loader_;
};

}  // namespace data_sharing

#endif  // COMPONENTS_DATA_SHARING_INTERNAL_DATA_SHARING_SERVICE_IMPL_H_
