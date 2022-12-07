// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_TEST_PERMISSION_REQUEST_OBSERVER_H_
#define COMPONENTS_PERMISSIONS_TEST_PERMISSION_REQUEST_OBSERVER_H_

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/permissions/permission_request_manager.h"

namespace permissions {

// Observer for use in tests that want to detect whether a permission request
// was shown.  Can be used in a blocking or non-blocking way.
class PermissionRequestObserver : public PermissionRequestManager::Observer {
 public:
  explicit PermissionRequestObserver(content::WebContents* web_contents);
  PermissionRequestObserver(const PermissionRequestObserver&) = delete;
  PermissionRequestObserver& operator=(const PermissionRequestObserver&) =
      delete;
  ~PermissionRequestObserver() override;

  bool request_shown() const { return request_shown_; }
  bool is_view_recreate_failed() const { return is_view_recreate_failed_; }
  bool is_prompt_show_failed_hidden_tab() const {
    return is_prompt_show_failed_hidden_tab_;
  }

  // Blocks until a request is shown.
  void Wait();

  // PermissionRequestManager::Observer:
  void OnPromptAdded() override;
  void OnRequestsFinalized() override;
  void OnPromptRecreateViewFailed() override;
  void OnPromptCreationFailedHiddenTab() override;
  void OnPermissionRequestManagerDestructed() override;

 private:
  base::ScopedObservation<PermissionRequestManager,
                          PermissionRequestManager::Observer>
      observation_{this};
  base::RunLoop loop_;
  bool request_shown_ = false;
  bool is_view_recreate_failed_ = false;
  bool is_prompt_show_failed_hidden_tab_ = false;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_TEST_PERMISSION_REQUEST_OBSERVER_H_
