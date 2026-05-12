// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SKILLS_INTERNAL_SKILLS_FETCHER_H_
#define COMPONENTS_SKILLS_INTERNAL_SKILLS_FETCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/skills/public/skills_types.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace skills {

// SkillsFetcher fetches the list of 1P skills from the Boq API using OAuth.
class SkillsFetcher {
 public:
  using OnFetchCompleteCallback =
      base::OnceCallback<void(std::unique_ptr<FirstPartySkillData>)>;

  SkillsFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~SkillsFetcher();

  // Initiates the async fetch process.
  void FetchDiscoverySkills(OnFetchCompleteCallback callback);

 private:
  void OnResponseFetched(
      OnFetchCompleteCallback callback,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> endpoint_fetcher_;

  base::WeakPtrFactory<SkillsFetcher> weak_ptr_factory_{this};
};

}  // namespace skills

#endif  // COMPONENTS_SKILLS_INTERNAL_SKILLS_FETCHER_H_
