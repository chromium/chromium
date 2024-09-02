// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_management_api_fetcher.h"

#include "base/version_info/channel.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace supervised_user {

std::unique_ptr<ClassifyUrlFetcher> CreateClassifyURLFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::ClassifyUrlRequest& request,
    const FetcherConfig& config,
    version_info::Channel channel) {
  return CreateFetcher<kidsmanagement::ClassifyUrlResponse>(
      identity_manager, url_loader_factory, request, config, /*args=*/{},
      channel);
}

std::unique_ptr<ListFamilyMembersFetcher> FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ListFamilyMembersFetcher::Callback callback,
    const FetcherConfig& config) {
  kidsmanagement::ListMembersRequest request;
  std::unique_ptr<ListFamilyMembersFetcher> fetcher =
      CreateFetcher<kidsmanagement::ListMembersResponse>(
          identity_manager, url_loader_factory, request, config);
  fetcher->Start(std::move(callback));
  return fetcher;
}

std::unique_ptr<PermissionRequestFetcher> CreatePermissionRequestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::PermissionRequest& request,
    const FetcherConfig& config) {
  return CreateFetcher<kidsmanagement::CreatePermissionRequestResponse>(
      identity_manager, url_loader_factory, request, config);
}
}  // namespace supervised_user
