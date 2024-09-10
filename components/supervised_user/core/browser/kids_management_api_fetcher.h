// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_API_FETCHER_H_
#define COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_API_FETCHER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace signin {
class IdentityManager;
}

namespace version_info {
enum class Channel;
}

namespace supervised_user {

using ClassifyUrlFetcher = ProtoFetcher<kidsmanagement::ClassifyUrlResponse>;
using ListFamilyMembersFetcher =
    ProtoFetcher<kidsmanagement::ListMembersResponse>;
using PermissionRequestFetcher =
    ProtoFetcher<kidsmanagement::CreatePermissionRequestResponse>;

// Fetches list family members. The returned fetcher is immediately started.
std::unique_ptr<ProtoFetcher<kidsmanagement::ListMembersResponse>>
FetchListFamilyMembers(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    ListFamilyMembersFetcher::Callback callback,
    const FetcherConfig& config = kListFamilyMembersConfig);

// Creates a disposable instance of an access token consumer that will classify
// the URL for supervised user.
std::unique_ptr<ClassifyUrlFetcher> CreateClassifyURLFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::ClassifyUrlRequest& request,
    ClassifyUrlFetcher::Callback callback,
    const FetcherConfig& config = kClassifyUrlConfig,
    version_info::Channel channel = version_info::Channel::UNKNOWN);

// Creates a disposable instance of an access token consumer that will create
// a new permission request for a given url.
// The fetcher does not need to use the `CreatePermissionRequestRequest`
// message. The `request` input corresponds to a `PermissionRequest` message,
// which is mapped to the body of the `CreatePermissionRequestRequest`
// message by the http to gRPC mapping on the server side.
// See go/rpc-create-permission-request.
std::unique_ptr<PermissionRequestFetcher> CreatePermissionRequestFetcher(
    signin::IdentityManager& identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const kidsmanagement::PermissionRequest& request,
    PermissionRequestFetcher::Callback callback,
    const FetcherConfig& config = kCreatePermissionRequestConfig);
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_BROWSER_KIDS_MANAGEMENT_API_FETCHER_H_
