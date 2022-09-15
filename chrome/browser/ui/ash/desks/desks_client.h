// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_DESKS_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DESKS_DESKS_CLIENT_H_

#include <map>
#include <memory>

#include "ash/public/cpp/session/session_observer.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/desks_storage/core/desk_model.h"
#include "components/sessions/core/session_id.h"

class DesksTemplatesAppLaunchHandler;
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

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TODO: Change the callback to accept a ash::DeskTemplate* type parameter
  // later when DesksTemplatesClient (or DesksController) hooks up with storage
  // and can hold an in-memory captured desk template instance.
  using CaptureActiveDeskAndSaveTemplateCallback =
      base::OnceCallback<void(std::unique_ptr<ash::DeskTemplate>,
                              std::string error)>;
  // Captures the active desk and saves it as template or saved desk for later
  // use. If such desk can be saved, `callback` will be invoked
  // with `""` as the error string with the pointer to the captured desk
  // template, otherwise, `callback` will be invoked with a description of the
  // error as the `error` with a nullptr.
  void CaptureActiveDeskAndSaveTemplate(
      CaptureActiveDeskAndSaveTemplateCallback callback,
      ash::DeskTemplateType template_type);

  using UpdateDeskTemplateCallback =
      base::OnceCallback<void(std::string error)>;
  // Updates the existing saved desk template with id |template_uuid| with the
  // new provided template name |template_name|. The |template_uuid| should be
  // the id of an existing desk template that was previously-saved in the
  // storage. If no such existing desk template can be found or the file
  // operation has failed, |callback| will be invoked with a description of the
  // error as the |error|.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void UpdateDeskTemplate(const base::GUID& template_uuid,
                          const std::u16string& template_name,
                          UpdateDeskTemplateCallback callback);

  using DeleteDeskTemplateCallback =
      base::OnceCallback<void(std::string error)>;
  // Deletes a saved desk template from storage. If the template can't be
  // deleted, |callback| will be invoked with a description of the error.
  // If it can be deleted successfully, or there is no such |template_uuid|
  // to be removed,|callback| will be invoked with an empty error string.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void DeleteDeskTemplate(const base::GUID& template_uuid,
                          DeleteDeskTemplateCallback callback);

  using GetDeskTemplatesCallback =
      base::OnceCallback<void(const std::vector<const ash::DeskTemplate*>&,
                              std::string error)>;
  // Returns the current available saved desk templates.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void GetDeskTemplates(GetDeskTemplatesCallback callback);

  using GetAllDesksCallback =
      base::OnceCallback<void(const std::vector<const ash::Desk*>&,
                              std::string error)>;
  // Returns the current available desks.
  void GetAllDesks(GetAllDesksCallback callback);

  using GetTemplateJsonCallback =
      base::OnceCallback<void(const std::string& template_json,
                              std::string error)>;
  // Takes in |uuid| and fetches the stringified json representation of a
  // desk template.
  void GetTemplateJson(const base::GUID& uuid,
                       Profile* profile,
                       GetTemplateJsonCallback callback);

  using LaunchDeskCallback =
      base::OnceCallback<void(std::string error, const base::GUID& desk_uuid)>;
  // Launches the desk template with `template_uuid` as a new desk.
  // `template_uuid` should be the unique id for an existing desk template. If
  // no such id can be found or we are at the max desk limit (currently is 8)
  // so can't create new desk for the desk template, `callback` will be invoked
  // with a description of the error and the new desk uuid. If
  // `customized_desk_name` is provided, desk name will be set to
  // `customized_desk_name` or `customized_desk_name ({counter})` to resolve
  // naming conflicts. Otherwise, desk name will be set to auto generated name.
  // TODO(crbug.com/1286515): This will be removed with the extension. Avoid
  // further uses of this method.
  void LaunchDeskTemplate(
      const base::GUID& template_uuid,
      LaunchDeskCallback callback,
      const std::u16string& customized_desk_name = std::u16string());

  // Launches an empty new desk. Desk name will be set to `customized_desk_name`
  // variant if it's provided, otherwise will be set to auto generated name.
  void LaunchEmptyDesk(
      LaunchDeskCallback callback,
      const std::u16string& customized_desk_name = std::u16string());

  using ErrorHandlingCallBack = base::OnceCallback<void(std::string error)>;
  // Remove a desk, close all windows if `close_all` set to true, otherwise
  // combine the windows to the active desk to the left.
  void RemoveDesk(const base::GUID& desk_uuid,
                  bool close_all,
                  ErrorHandlingCallBack);

  // Uses `app_launch_handler_` to launch apps from the restore data found in
  // `desk_template`.
  void LaunchAppsFromTemplate(std::unique_ptr<ash::DeskTemplate> desk_template);

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
  void SetAllDeskPropertyByBrowserSessionId(SessionID browser_session_id,
                                            bool all_desk,
                                            ErrorHandlingCallBack);

 private:
  class LaunchPerformanceTracker;
  friend class DesksTemplatesClientTest;
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
      std::unique_ptr<ash::DeskTemplate> desk_template,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status);

  // Callback function that allows for the |DeleteDeskTemplateCallback| to be
  // called as a |desks_storage::DeleteEntryCallback|
  void OnDeleteDeskTemplate(DeleteDeskTemplateCallback callback,
                            desks_storage::DeskModel::DeleteEntryStatus status);
  // Callback function that is run after a saved desk called and moved from
  // library.
  void OnRecallSavedDesk(DesksClient::LaunchDeskCallback callback,
                         const base::GUID& desk_id,
                         desks_storage::DeskModel::DeleteEntryStatus status);

  // Callback function that allows the |UpdateDeskTemplateCallback| to be called
  // as a |desks_storage::AddOrUpdateEntryCallback|.
  void OnUpdateDeskTemplate(
      UpdateDeskTemplateCallback callback,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status);

  // Callback function that is called once the DesksController has captured the
  // active desk as a template. Invokes |callback| with |desk_template| as an
  // argument.
  void OnCapturedDeskTemplate(CaptureActiveDeskAndSaveTemplateCallback callback,
                              std::unique_ptr<ash::DeskTemplate> desk_template);

  // Callback function that handles the JSON representation of a specific
  // template.
  void OnGetTemplateJson(DesksClient::GetTemplateJsonCallback callback,
                         desks_storage::DeskModel::GetTemplateJsonStatus status,
                         const std::string& json_representation);

  // Callback function that clears the data associated with a specific launch.
  void OnLaunchComplete(int32_t launch_id);

  // Called by a launch performance tracker when it has completed monitoring the
  // launch of a template.
  void RemoveLaunchPerformanceTracker(base::GUID tracker_uuid);

  // Get the pointer to the window by `browser_session_id`.
  aura::Window* GetWindowByBrowserSessionId(SessionID browser_session_id);

  // Convenience pointer to ash::DesksController. Guaranteed to be not null for
  // the duration of `this`.
  ash::DesksController* const desks_controller_;

  Profile* active_profile_ = nullptr;

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

  // The stored JSON values of preconfigured desk templates
  base::flat_map<AccountId, std::string> preconfigured_desk_templates_json_;

  // Mapping of template ids that are being launched to their launch performance
  // trackers.
  base::flat_map<base::GUID, std::unique_ptr<LaunchPerformanceTracker>>
      template_ids_to_launch_performance_trackers_;

  base::WeakPtrFactory<DesksClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_DESKS_TEMPLATES_CLIENT_H_
