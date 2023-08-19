// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_IMPL_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/sharing/password_sharing_recipients_downloader.h"
#include "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#include "components/version_info/channel.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace password_manager {

// The class is the implementation of the `RecipientFetcher` interface.
class RecipientsFetcherImpl : public RecipientsFetcher {
 public:
  explicit RecipientsFetcherImpl(
      version_info::Channel channel,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      raw_ptr<signin::IdentityManager> identity_manager);
  RecipientsFetcherImpl(const RecipientsFetcherImpl&) = delete;
  RecipientsFetcherImpl& operator=(const RecipientsFetcherImpl&) = delete;
  ~RecipientsFetcherImpl() override;

  // Fetches the Family Members including status about their capability for
  // receiving Passwords.
  // If a call to the server is already in flight, no new call will be issued
  // for further consecutive method calls. The callback of concurrent calls will
  // be run immediately with the status set to kPendingRequest, indicating that
  // there is already an active request in flight.
  void FetchFamilyMembers(FetchFamilyMembersCallback callback) override;

 private:
  void ServerRequestCallback();

  const version_info::Channel channel_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  FetchFamilyMembersCallback callback_;
  std::unique_ptr<PasswordSharingRecipientsDownloader> pending_request_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SHARING_RECIPIENTS_FETCHER_IMPL_H_
