// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_DESKS_TEMPLATES_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_DESKS_TEMPLATES_CLIENT_H_

#include <memory>

#include "ash/public/cpp/desk_template.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/desks_storage/core/desk_model.h"

class DesksTemplatesAppLaunchHandler;

namespace ash {
class DeskTemplate;
class DesksController;
}  // namespace ash

namespace desks_storage {
class DeskModel;
class LocalDeskDataManager;
}  // namespace desks_storage

// Class to handle all Desks in-browser functionalities. Will call into
// ash::DesksController to do actual desk related operations.
class DesksTemplatesClient : public ash::SessionObserver {
 public:
  DesksTemplatesClient();
  DesksTemplatesClient(const DesksTemplatesClient&) = delete;
  DesksTemplatesClient& operator=(const DesksTemplatesClient&) = delete;
  ~DesksTemplatesClient() override;

  static DesksTemplatesClient* Get();

  // ash::SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TODO: Change the callback to accept a ash::DeskTemplate* type parameter
  // later when DesksTemplatesClient (or DesksController) hooks up with storage
  // and can hold an in-memory captured desk template instance.
  using CaptureActiveDeskAndSaveTemplateCallback =
      base::OnceCallback<void(std::unique_ptr<ash::DeskTemplate>,
                              std::string error)>;
  // Captures the active desk as a template and saves the template to storage.
  // If such template can be created and saved, |callback| will be invoked with
  // |""| as the error string with the pointer to the captured desk template,
  // otherwise, |callback| will be invoked with a description of the error as
  // the |error| with a nullptr.
  void CaptureActiveDeskAndSaveTemplate(
      CaptureActiveDeskAndSaveTemplateCallback callback);

  using UpdateDeskTemplateCallback =
      base::OnceCallback<void(std::string error)>;
  // Updates the existing saved desk template with id |template_uuid| with the
  // new provided template name |template_name|. The |template_uuid| should be
  // the id of an existing desk template that was previously-saved in the
  // storage. If no such existing desk template can be found or the file
  // operation has failed, |callback| will be invoked with a description of the
  // error as the |error|.
  void UpdateDeskTemplate(const std::string& template_uuid,
                          const std::u16string& template_name,
                          UpdateDeskTemplateCallback callback);

  using DeleteDeskTemplateCallback =
      base::OnceCallback<void(std::string error)>;
  // Deletes a saved desk template from storage. If the template can't be
  // deleted, |callback| will be invoked with a description of the error.
  // If it can be deleted successfully, or there is no such |template_uuid|
  // to be removed,|callback| will be invoked with an empty error string.
  void DeleteDeskTemplate(const std::string& template_uuid,
                          DeleteDeskTemplateCallback callback);

  using TemplateList = std::vector<ash::DeskTemplate*>;
  using GetDeskTemplatesCallback =
      base::OnceCallback<void(const TemplateList&, std::string error)>;
  // Returns the current available saved desk templates.
  void GetDeskTemplates(GetDeskTemplatesCallback callback);

  using GetTemplateJsonCallback =
      base::OnceCallback<void(const std::string& template_json,
                              std::string error)>;
  // Takes in |uuid| and fetches the stringified json representation of a
  // desk template.
  void GetTemplateJson(const std::string uuid,
                       Profile* profile,
                       GetTemplateJsonCallback callback);

  using LaunchDeskTemplateCallback =
      base::OnceCallback<void(const std::string error)>;
  // Launches the desk template with |template_uuid| as a new desk.
  // |template_uuid| should be the unique id for an existing desk template. If
  // no such id can be found or we are at the max desk limit (currently is 8)
  // so can't create new desk for the desk template, |callback| will be invoked
  // with a description of the error.
  void LaunchDeskTemplate(const std::string& template_uuid,
                          LaunchDeskTemplateCallback callback);

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

 private:
  friend class DesksTemplatesClientTest;
  friend class ScopedDesksTemplatesAppLaunchHandlerSetter;

  // Attempts to create `app_launch_handler_` if it doesn't already exist.
  void MaybeCreateAppLaunchHandler();

  void RecordWindowAndTabCountHistogram(ash::DeskTemplate* desk_template);
  void RecordLaunchFromTemplateHistogram();
  void RecordTemplateCountHistogram();

  // Launches DeskTemplate after retrieval from storage.
  void OnGetTemplateForDeskLaunch(
      LaunchDeskTemplateCallback callback,
      desks_storage::DeskModel::GetEntryByUuidStatus status,
      std::unique_ptr<ash::DeskTemplate> entry);

  // Callback function that is ran after a desk is created, or has failed to be
  // created.
  void OnCreateAndActivateNewDesk(
      std::unique_ptr<ash::DeskTemplate> desk_template,
      LaunchDeskTemplateCallback callback,
      bool on_create_activate_success);

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

  // Callback function that allows the |UpdateDeskTemplateCallback| to be called
  // as a |desks_storage::AddOrUpdateEntryCallback|.
  void OnUpdateDeskTemplate(
      UpdateDeskTemplateCallback callback,
      desks_storage::DeskModel::AddOrUpdateEntryStatus status);

  // Callback function that handles finding a template to be updated in
  // |UpdateDeskTemplate|
  void OnGetTemplateToBeUpdated(
      const std::u16string& template_name,
      UpdateDeskTemplateCallback callback,
      desks_storage::DeskModel::GetEntryByUuidStatus status,
      std::unique_ptr<ash::DeskTemplate> entry);

  // Callback function that handles getting all DeskTemplates from
  // storage.
  void OnGetAllTemplates(GetDeskTemplatesCallback callback,
                         desks_storage::DeskModel::GetAllEntriesStatus status,
                         const std::vector<ash::DeskTemplate*>& entries);

  // Callback function that is called once the DesksController has captured the
  // active desk as a template. Invokes |callback| with |desk_template| as an
  // argument.
  void OnCapturedDeskTemplate(CaptureActiveDeskAndSaveTemplateCallback callback,
                              std::unique_ptr<ash::DeskTemplate> desk_template);

  // Callback function that handles the JSON representation of a specific
  // template.
  void OnGetTemplateJson(DesksTemplatesClient::GetTemplateJsonCallback callback,
                         desks_storage::DeskModel::GetTemplateJsonStatus status,
                         const std::string& json_representation);

  // Convenience pointer to ash::DesksController.
  // Guaranteed to be not null for the duration of `this`.
  ash::DesksController* const desks_controller_;

  Profile* active_profile_ = nullptr;

  // The object that handles launching apps.
  std::unique_ptr<DesksTemplatesAppLaunchHandler> app_launch_handler_;

  // A test only template for testing `LaunchDeskTemplate`.
  std::unique_ptr<ash::DeskTemplate> launch_template_for_test_;

  // Local desks storage backend.
  std::unique_ptr<desks_storage::LocalDeskDataManager> storage_manager_;

  // The stored JSON values of preconfigured desk templates
  base::flat_map<AccountId, std::string> preconfigured_desk_templates_json_;

  base::WeakPtrFactory<DesksTemplatesClient> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_DESKS_TEMPLATES_DESKS_TEMPLATES_CLIENT_H_
