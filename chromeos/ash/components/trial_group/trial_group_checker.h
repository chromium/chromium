// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TRIAL_GROUP_TRIAL_GROUP_CHECKER_H_
#define CHROMEOS_ASH_COMPONENTS_TRIAL_GROUP_TRIAL_GROUP_CHECKER_H_

#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace ash {
namespace trial_group {

// TrialGroupChecker determines whether the user is in a particular dogfood
// trial by asking the external Dogpack server. The caller should only make
// this request for users that have a dogfood finch experiment flag set.
// |group_id| contains the integer corresponding to the dogfood trial. Only one
// |group_id| per instance. To check another |group_id| use another instance.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_TRIAL_GROUP) TrialGroupChecker {
 public:
  enum GroupId {
    INVALID_GROUP = 0,
    ATLAS_DOGFOOD_GROUP = 1,
    TESTING_GROUP = 2,
  };

  enum Status {
    OK,                     // Everything went as planned.
    PREVIOUS_CALL_RUNNING,  // Aborted due to previous call still running.
  };

  explicit TrialGroupChecker(GroupId group_id);

  TrialGroupChecker(const TrialGroupChecker&) = delete;
  TrialGroupChecker& operator=(const TrialGroupChecker&) = delete;

  ~TrialGroupChecker();

  // Checks user's membership and passes the result to a callback. The
  // TrialGroupChecker instance must live until after |callback| has finished
  // executing. LookUpMembership must not be called again until |callback| from
  // the previous call has completed.
  Status LookUpMembership(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::OnceCallback<void(bool is_member)> callback);

  // SetServerUrl is only used for testing.
  void SetServerUrl(GURL server_url);

 private:
  void OnRequestComplete(std::unique_ptr<std::string> response_body);

  // The url of the Dogpack server.
  GURL server_url_;
  // The id of the Google Group.
  int group_id_;
  // The callback provided by the caller.
  base::OnceCallback<void(bool is_member)> callback_;
  // Loader that sends the HTTP request to the Dogpack server.
  std::unique_ptr<network::SimpleURLLoader> loader_;

  base::WeakPtrFactory<TrialGroupChecker> weak_factory_{this};
};

}  // namespace trial_group
}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TRIAL_GROUP_TRIAL_GROUP_CHECKER_H_
