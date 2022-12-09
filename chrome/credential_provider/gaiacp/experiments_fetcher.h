// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_FETCHER_H_
#define CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_FETCHER_H_

#include "base/values.h"
#include "base/win/windows_types.h"
#include "chrome/credential_provider/extension/task_manager.h"
#include "url/gurl.h"

namespace credential_provider {

// Fetcher used to fetch experiments from GCPW backends.
class ExperimentsFetcher {
 public:
  // Gets the singleton instance.
  static ExperimentsFetcher* Get();

  // Provides the GCPW extension with a TaskCreator which can be used to create
  // a task for fetching experiments.
  static extension::TaskCreator GetFetchExperimentsTaskCreator();

  // Fetches the experiments for the user implied with the |access_token| from
  // the GCPW backend and saves it in file storage replacing any previously
  // fetched versions.
  HRESULT FetchAndStoreExperiments(const std::wstring& sid,
                                   const std::string& access_token);

  // Fetches the experiments for the user-device |context| provided by the GCPW
  // extension service from the GCPW backend and saves it in file storage
  // replacing any previously fetched versions.
  HRESULT FetchAndStoreExperiments(const extension::UserDeviceContext& context);

  // Returns the experiments fetch endpoint.
  GURL GetExperimentsUrl();

 private:
  // Returns the storage used for the instance pointer.
  static ExperimentsFetcher** GetInstanceStorage();

  ExperimentsFetcher();
  virtual ~ExperimentsFetcher();

  // Fetches experiments with the provided |request_dict|. |access_token| needs
  // to be present if the experiments are being fetched in the presence of an
  // oauth token. Otherwise |request_dict| should be carrying the obfuscated
  // user id as well as DM token.
  HRESULT FetchAndStoreExperimentsInternal(
      const std::wstring& sid,
      const std::string& access_token,
      std::unique_ptr<base::Value::Dict> request_dict);
};

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_GAIACP_EXPERIMENTS_FETCHER_H_
