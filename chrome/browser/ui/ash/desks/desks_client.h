// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_DESKS_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DESKS_DESKS_CLIENT_H_

#include <map>
#include <memory>

#include "ash/public/cpp/session/session_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/sessions/core/session_id.h"

class DesksTemplatesAppLaunchHandler;
class LacrosAppWindowObserver;
class Profile;

namespace ash {
class Desk;
class DeskTemplate;
class DesksController;
}  // namespace ash

namespace aura {
class Window;
}  // namespace aura

namespace desks_storage {
class DeskModel;
class LocalDeskDataManager;
class DeskModelWrapper;
}  // namespace desks_storage

// Class to handle all Desks in-browser functionalities. Will call into
// ash::DesksController to do actual desk related operations.
class DesksClient : public ash::SessionObserver {
 public:
  DesksClient();
  DesksClient(const DesksClient&) = delete;
  DesksClient& operator=(const DesksClient&) = delete;
  ~DesksClient() override;

  static DesksClient* Get();

  enum class DeskActionError {
    // Unknown error.
    kUnknownError = 0,
    // Storage error.
    kStorageError = 1,
    // Therer is no active profile.
    kNoCurrentUserError = 2,
    // Either the profile is not valid or there is not an active profile.
    kBadProfileError = 3,
    // The resource cannot be found.
    kResourceNotFoundError = 4,
    // The identifier is not valid.
    kInvalidIdError = 5,
    // The desks are currently being modified.
    kDesksBeingModifiedError = 6,
    // The desk count requirement not met.
    kDesksCountCheckFailedError = 7,
    kMaxValue = kDesksCountCheckFailedError,
  };

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TODO: Change the callback to accept a ash::DeskTemplate* type parameter
  // later when DesksTemplatesClient (or DesksController) hooks up with storage
  // and can hold an in-memory captured desk template instance.
  using CaptureActiveDeskAndSaveTemplateCallback =
      base::OnceCallback<void(std::optional<DeskActionError> result,
                              std::unique_ptr<ash::DeskTemplate>)>;

  // Captures the active desk and saves it as template or saved desk for later
  // use. If such desk can be saved, `callback` will be invoked with
  // `std::nullopt` as the `result` with the pointer to the captured desk
  // template, otherwise, `callback` will be invoked with an `DeskActionError`
  // error as the `result` and a nullptr for desk template.
  void CaptureActiveDeskAndSaveTemplate(
      CaptureActiveDeskAndSaveTemplateCallback callback,
      ash::DeskTemplateType template_type);

  // Captures the active desk without saving it. If such desk can be saved,
  // `callback` will be invoked with `std::nullopt` as the `result` with the
  // pointer to the captured desk template, otherwise, `callback` will be
  // invoked with an `DeskActionError` error as the `result` and a nullptr for
  // desk template.
  virtual void CaptureActiveDesk(
      CaptureActiveDeskAndSaveTemplateCallback callback,
      ash::DeskTemplateType template_type);

  using DeleteDeskTemplateCallback =
      base::OnceCallback<void(std::optional<DeskActionError> result)>;
  // Deletes a saved desk template from storage. If the template can't be
  // deleted, |callback| will be invoked with the error code.
  // If it can be deleted successfully, or there is no such |template_uuid|
  // to be removed,|callback| will be invoked with the success result code.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void DeleteDeskTemplate(const base::Uuid& template_uuid,
                          DeleteDeskTemplateCallback callback);

  using GetDeskTemplatesCallback =
      base::OnceCallback<void(std::optional<DeskActionError> result,
                              const std::vector<raw_ptr<const ash::DeskTemplate,
                                                        VectorExperimental>>&)>;
  // Returns the current available saved desk templates.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void GetDeskTemplates(GetDeskTemplatesCallback callback);

  // Returns the current available desks.
  virtual base::expected<std::vector<const ash::Desk*>, DeskActionError>
  GetAllDesks();

  using GetTemplateJsonCallback =
      base::OnceCallback<void(std::optional<DeskActionError> result,
                              const base::Value& template_json)>;
  // Takes in |uuid| and fetches the stringified json representation of a
  // desk template.
  void GetTemplateJson(const base::Uuid& uuid,
                       Profile* profile,
                       GetTemplateJsonCallback callback);

  using LaunchDeskCallback =
      base::OnceCallback<void(std::optional<DeskActionError> result,
                              const base::Uuid& desk_uuid)>;
  // Launches the desk template with `template_uuid` as a new desk.
  // `template_uuid` should be the unique id for an existing desk template. If
  // no such id can be found or we are at the max desk limit (currently is 8)
  // so can't create new desk for the desk template, `callback` will be invoked
  // with a the error code. If `customized_desk_name` is provided, desk name
  // will be set to `customized_desk_name` or `customized_desk_name ({counter})`
  // to resolve naming conflicts. Otherwise, desk name will be set to auto
  // generated name.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  virtual void LaunchDeskTemplate(
      const base::Uuid& template_uuid,
      LaunchDeskCallback callback,
      const std::u16string& customized_desk_name = std::u16string());

  // Launches an empty new desk. Desk name will be set to `customized_desk_name`
  // variant if it's provided, otherwise will be set to auto generated name.
  base::expected<const base::Uuid, DeskActionError> LaunchEmptyDesk(
      const std::u16string& customized_desk_name = std::u16string());

  using ErrorHandlingCallBack =
      base::OnceCallback<void(std::optional<DeskActionError> result)>;
  // Remove a desk, close all windows if `close_type` set to kCloseAllWindows,
  // otherwise combine the windows to the active desk to the left. Provide
  // a notification allowing the user to undo the removal if `close_type` is
  // set to `kCloseAllWindowsAndWait`
  virtual std::optional<DesksClient::DeskActionError> RemoveDesk(
      const base::Uuid& desk_uuid,
      ash::DeskCloseType close_type);

  // Uses `app_launch_handler_` to launch apps from the restore data found in
  // `desk_template`.
  virtual void LaunchAppsFromTemplate(
      std::unique_ptr<ash::DeskTemplate> desk_template);

  // Returns either the local desk storage backend or Chrome sync desk storage
  // backend depending on the feature flag DeskTemplateSync.
  desks_storage::DeskModel* GetDeskModel();

  // Sets the preconfigured desk template.
  void SetPolicyPreconfiguredTemplate(const AccountId& account_id,
                                      std::unique_ptr<std::string> data);
  void RemovePolicyPreconfiguredTemplate(const AccountId& account_id);

  // Notifies launch performance trackers that an app has been moved rather
  // than launched.
  void NotifyMovedSingleInstanceApp(int32_t window_id);

  // Set the property of showing on all-desk or not to a window.
  std::optional<DesksClient::DeskActionError>
  SetAllDeskPropertyByBrowserSessionId(SessionID browser_session_id,
                                       bool all_desk);

  // Returns the UUID of active desk.
  virtual base::Uuid GetActiveDesk();

  // Retrieves desk by its UUID.
  virtual base::expected<const ash::Desk*, DesksClient::DeskActionError>
  GetDeskByID(const base::Uuid& desk_uuid) const;

  // Switches to the target desk, returns error string if operation fails.
  std::optional<DesksClient::DeskActionError> SwitchDesk(
      const base::Uuid& desk_uuid);

  // If `window` is a lacros window that has an app id, return it.
  std::optional<std::string> GetAppIdForLacrosWindow(
      aura::Window* window) const;

 private:
  class LaunchPerformanceTracker;
  class DeskEventObserver;
  friend class DesksClientTest;
  friend class ScopedDesksTemplatesAppLaunchHandlerSetter;

  // Launches DeskTemplate after retrieval from storage.
  void OnGetTemplateForDeskLaunch(
      LaunchDeskCallback callback,
      std::u16string customized_desk_name,
      desks_storage::DeskModel::GetEntryByUuidStatus status,
      std::unique_ptr<ash::DeskTemplate> saved_desk);

  // Callback function that allows the |CaptureActiveDeskAndSaveTemplate|
  // |callback| to be called as a |desks_storage::AddOrUpdateEntryCallback|.
  void OnCaptureActiveDeskAndSaveTemplate(
      CaptureActiveDeskAndSaveTemplateCallback callback,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status,
      std::unique_ptr<ash::DeskTemplate> desk_template);

  // Callback function that allows for the |DeleteDeskTemplateCallback| to be
  // called as a |desks_storage::DeleteEntryCallback|
  void OnDeleteDeskTemplate(DeleteDeskTemplateCallback callback,
                            desks_storage::DeskModel::DeleteEntryStatus status);
  // Callback function that is run after a saved desk called and moved from
  // library.
  void OnRecallSavedDesk(DesksClient::LaunchDeskCallback callback,
                         const base::Uuid& desk_id,
                         desks_storage::DeskModel::DeleteEntryStatus status);

  // Callback function that is called once the DesksController has captured the
  // active desk as a template. Invokes |callback| with |desk_template| as an
  // argument.
  void OnCapturedDeskTemplate(CaptureActiveDeskAndSaveTemplateCallback callback,
                              std::optional<DesksClient::DeskActionError> error,
                              std::unique_ptr<ash::DeskTemplate> desk_template);

  // Callback function that handles the JSON representation of a specific
  // template.
  void OnGetTemplateJson(DesksClient::GetTemplateJsonCallback callback,
                         desks_storage::DeskModel::GetTemplateJsonStatus status,
                         const base::Value& json_representation);

  // Callback function that clears the data associated with a specific launch.
  void OnLaunchComplete(int32_t launch_id);

  // Called by a launch performance tracker when it has completed monitoring the
  // launch of a template.
  void RemoveLaunchPerformanceTracker(const base::Uuid& tracker_uuid);

  // Get the pointer to the window by `browser_session_id`.
  aura::Window* GetWindowByBrowserSessionId(SessionID browser_session_id);

  // Creates a new desk and switch to it. If `customized_desk_name` is
  // provided, desk name will be `customized_desk_name` or `customized_desk_name
  // ({counter})` to resolve naming conflicts. CanCreateDesks() must be checked
  // before calling this.
  const ash::Desk* CreateEmptyDeskAndActivate(
      const std::u16string& customized_desk_name);

  // Convenience pointer to ash::DesksController. Guaranteed to be not null for
  // the duration of `this`.
  const raw_ptr<ash::DesksController> desks_controller_;

  raw_ptr<Profile> active_profile_ = nullptr;

  // Maps launch id to a launch handler.
  std::map<int32_t, std::unique_ptr<DesksTemplatesAppLaunchHandler>>
      app_launch_handlers_;

  // A test only template for testing `LaunchDeskTemplate`.
  std::unique_ptr<ash::DeskTemplate> launch_template_for_test_;

  // Local desks storage backend for desk templates.
  std::unique_ptr<desks_storage::LocalDeskDataManager>
      desk_templates_storage_manager_;

  // Local desks storage backend for save and recall desks.
  std::unique_ptr<desks_storage::LocalDeskDataManager>
      save_and_recall_desks_storage_manager_;

  // Wrapper desk model to house both desk types backend storage.
  std::unique_ptr<desks_storage::DeskModelWrapper> saved_desk_storage_manager_;

  // Monitors lacros app windows for use in saved desks.
  std::unique_ptr<LacrosAppWindowObserver> lacros_app_window_observer_;

  // The stored JSON values of preconfigured desk templates
  base::flat_map<AccountId, std::string> preconfigured_desk_templates_json_;

  // Mapping of template ids that are being launched to their launch performance
  // trackers.
  base::flat_map<base::Uuid, std::unique_ptr<LaunchPerformanceTracker>>
      template_ids_to_launch_performance_trackers_;

  // Monitors desk events.
  std::unique_ptr<DeskEventObserver> desk_event_observer_;

  base::WeakPtrFactory<DesksClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_DESKS_CLIENT_H_
