// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_FETCHER_H_
#define COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/policy_export.h"

namespace policy {

class ExternalDataManager;

// A helper that encapsulates the parameters required to retrieve the external
// data for a policy.
class POLICY_EXPORT ExternalDataFetcher {
 public:
  using FetchCallback = base::OnceCallback<void(std::unique_ptr<std::string>,
                                                const base::FilePath&)>;

  // This instance's Fetch() method will instruct the |manager| to retrieve the
  // external data referenced by the given |policy|.
  ExternalDataFetcher(base::WeakPtr<ExternalDataManager> manager,
                      const std::string& policy);
  ExternalDataFetcher(const ExternalDataFetcher& other);

  ~ExternalDataFetcher();

  static bool Equals(const ExternalDataFetcher* first,
                     const ExternalDataFetcher* second);

  // Retrieves the external data referenced by |policy_| and invokes |callback|
  // with the result. If |policy_| does not reference any external data, the
  // |callback| is invoked with a NULL pointer. Otherwise, the |callback| is
  // invoked with the referenced data once it has been successfully retrieved.
  // If retrieval is temporarily impossible (e.g. no network connectivity), the
  // |callback| will be invoked when the temporary hindrance is resolved. If
  // retrieval is permanently impossible (e.g. |policy_| references data that
  // does not exist on the server), the |callback| will never be invoked.
  void Fetch(FetchCallback callback) const;

  // Same as above, except there might be multiple pieces of data associated
  // with the |policy_|. |field_name| specifies which specific data should be
  // fetched. The relation between |field_name| and the policy value is
  // policy-specific.
  void Fetch(const std::string& field_name, FetchCallback callback) const;

 private:
  base::WeakPtr<ExternalDataManager> manager_;
  const std::string policy_;
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_EXTERNAL_DATA_FETCHER_H_
