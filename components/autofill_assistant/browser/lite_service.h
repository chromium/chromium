// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_LITE_SERVICE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_LITE_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/service_impl.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace autofill_assistant {

// This service is intended for just-in-time triggering. It supports only a
// subset of 'safe' actions. It is also limited to a single call to the backend
// via |GetActions|, which in turn is limited to sending the script path and no
// other information.
class LiteService : public Service {
 public:
  explicit LiteService(
      std::unique_ptr<Service> service_impl,
      const std::string& trigger_script_path,
      base::OnceCallback<void(Metrics::LiteScriptFinishedState)>
          notify_finished_callback,
      base::RepeatingCallback<void(bool)> notify_script_running_callback);
  // If the destructor is called before |GetNextActions|, the script was
  // terminated before finishing (user cancelled, closed the tab, etc.).
  ~LiteService() override;
  LiteService(const LiteService&) = delete;
  LiteService& operator=(const LiteService&) = delete;

  bool IsLiteService() const override;

  // |LiteService| will return the hard-coded response without communicating
  // with the backend.
  void GetScriptsForUrl(const GURL& url,
                        const TriggerContext& trigger_context,
                        ResponseCallback callback) override;

  // Get actions from regular backend. Can only be called once. Fails if any
  // of the returned actions is not in |allowed_actions_|.
  void GetActions(const std::string& script_path,
                  const GURL& url,
                  const TriggerContext& trigger_context,
                  const std::string& global_payload,
                  const std::string& script_payload,
                  ResponseCallback callback) override;

  // Signifies the end of the mini script. Reaching this implies that the mini
  // script was run to the end. Depending on the status of the last processed
  // action, the finished state is either LITE_SCRIPT_SUCCESS or
  // LITE_SCRIPT_ACTION_FAILED.
  void GetNextActions(
      const TriggerContext& trigger_context,
      const std::string& previous_global_payload,
      const std::string& previous_script_payload,
      const std::vector<ProcessedActionProto>& processed_actions,
      ResponseCallback callback) override;

 private:
  friend class LiteServiceTest;

  void OnGetActions(ResponseCallback callback,
                    bool result,
                    const std::string& response);

  // Stops the script and closes autofill assistant without showing an error
  // message. This is done by running an explicit stop action, followed by an
  // empty response in |GetNextActions|.
  void StopWithoutErrorMessage(ResponseCallback callback,
                               Metrics::LiteScriptFinishedState state);

  // The actual service that communicates with the backend.
  std::unique_ptr<Service> service_impl_;
  // The script path to fetch actions from.
  std::string trigger_script_path_;

  // Notifies the java bridge of the finished state.
  //
  // Note that this callback will be run BEFORE the controller shuts down. This
  // is necessary to transition between old and new bottom sheet contents.
  // Case 1: onboarding will be shown. The controller will terminate gracefully
  // with an explicit stop action executed after the callback was run.
  // Case 2: onboarding will not be shown. The controller must terminate
  // immediately and make way for the main script controller. Since the callback
  // is run while the old controller is still around, the caller can hot-swap
  // controllers and smoothly transition bottom sheet contents.
  // Case 3: the lite script failed (|state| != LITE_SCRIPT_SUCCESS). The
  // controller will terminate gracefully with an explicit stop action.
  base::OnceCallback<void(Metrics::LiteScriptFinishedState)>
      notify_finished_callback_;
  // Notifies the java bridge that the script is running. The bool parameter
  // indicates whether the UI is being shown or not.
  base::RepeatingCallback<void(bool)> notify_script_running_callback_;

  // The second part of the trigger script, i.e., the actions that should be run
  // after a successful prompt(browse) action in the first part of the script.
  std::unique_ptr<ActionsResponseProto> trigger_script_second_part_;

  base::WeakPtrFactory<LiteService> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_LITE_SERVICE_H_
