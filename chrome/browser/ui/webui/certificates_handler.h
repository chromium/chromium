// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/containers/id_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/certificate_manager_model.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "net/cert/nss_cert_database.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

#if defined(OS_CHROMEOS)
enum class Slot { kUser, kSystem };
enum class CertificateSource { kBuiltIn, kImported };

// Enumeration of certificate management permissions which corresponds to
// values of policy ClientCertificateManagementAllowed.
// Underlying type is int because values are casting to/from prefs values.
enum class ClientCertificateManagementPermission : int {
  // Allow users to manage all certificates
  kAll = 0,
  // Allow users to manage user certificates
  kUserOnly = 1,
  // Disallow users from managing certificates
  kNone = 2
};

// Enumeration of certificate management permissions which corresponds to
// values of policy CACertificateManagementAllowed.
// Underlying type is int because values are casting to/from prefs values.
enum class CACertificateManagementPermission : int {
  // Allow users to manage all certificates
  kAll = 0,
  // Allow users to manage user certificates
  kUserOnly = 1,
  // Disallow users from managing certificates
  kNone = 2
};
#endif  // defined(OS_CHROMEOS)

namespace certificate_manager {

class FileAccessProvider;

class CertificatesHandler : public content::WebUIMessageHandler,
                            public CertificateManagerModel::Observer,
                            public ui::SelectFileDialog::Listener {
 public:
  CertificatesHandler();
  ~CertificatesHandler() override;

  // content::WebUIMessageHandler.
  void RegisterMessages() override;

  // CertificateManagerModel::Observer implementation.
  void CertificatesRefreshed() override;

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

#if defined(OS_CHROMEOS)
  // Register profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
#endif

 private:
  // View certificate.
  void HandleViewCertificate(const base::ListValue* args);

  // Edit certificate authority trust values.  The sequence goes like:
  //  1. user clicks edit button -> Edit dialog is shown ->
  //  HandleGetCATrust -> Edit dialog is populated.
  //  2. User checks/unchecks boxes, and clicks ok -> HandleEditCATrust ->
  //  edit dialog is dismissed upon success.
  void HandleGetCATrust(const base::ListValue* args);
  void HandleEditCATrust(const base::ListValue* args);

  // Cleanup state stored during import or export process.
  void HandleCancelImportExportProcess(const base::ListValue* args);
  void ImportExportCleanup();

  // Export to PKCS #12 file.  The sequence goes like:
  //  1. user click on export button -> HandleExportPersonal -> launches file
  //  selector
  //  2. user selects file -> ExportPersonalFileSelected -> launches password
  //  dialog
  //  3. user enters password -> HandleExportPersonalPasswordSelected ->
  //  unlock slots
  //  4. slots unlocked -> ExportPersonalSlotsUnlocked -> exports to memory
  //  buffer -> starts async write operation
  //  5. write finishes (or fails) -> ExportPersonalFileWritten
  void HandleExportPersonal(const base::ListValue* args);
  void ExportPersonalFileSelected(const base::FilePath& path);
  void HandleExportPersonalPasswordSelected(const base::ListValue* args);
  void ExportPersonalSlotsUnlocked();
  void ExportPersonalFileWritten(const int* write_errno,
                                 const int* bytes_written);

  // Import from PKCS #12 or cert file.  The sequence goes like:
  //  1. user click on import button -> HandleImportPersonal ->
  //  launches file selector
  //  2. user selects file -> ImportPersonalFileSelected -> starts async
  //  read operation
  //  3. read operation completes -> ImportPersonalFileRead ->
  //    If file is PFX -> launches password dialog, goto step 4
  //    Else -> import as certificate, goto step 6
  //  4. user enters password -> HandleImportPersonalPasswordSelected ->
  //  unlock slot
  //  5. slot unlocked -> ImportPersonalSlotUnlocked attempts to
  //  import with previously entered password
  //  6a. if import succeeds -> ImportExportCleanup
  //  6b. if import fails -> show error, ImportExportCleanup
  //  TODO(mattm): allow retrying with different password
  void HandleImportPersonal(const base::ListValue* args);
  void ImportPersonalFileSelected(const base::FilePath& path);
  void ImportPersonalFileRead(const int* read_errno, const std::string* data);
  void HandleImportPersonalPasswordSelected(const base::ListValue* args);
  void ImportPersonalSlotUnlocked();

  // Import Server certificates from file.  Sequence goes like:
  //  1. user clicks on import button -> HandleImportServer -> launches file
  //  selector
  //  2. user selects file -> ImportServerFileSelected -> starts async read
  //  3. read completes -> ImportServerFileRead -> parse certs -> attempt import
  //  4a. if import succeeds -> ImportExportCleanup
  //  4b. if import fails -> show error, ImportExportCleanup
  void HandleImportServer(const base::ListValue* args);
  void ImportServerFileSelected(const base::FilePath& path);
  void ImportServerFileRead(const int* read_errno, const std::string* data);

  // Import Certificate Authorities from file.  Sequence goes like:
  //  1. user clicks on import button -> HandleImportCA -> launches file
  //  selector
  //  2. user selects file -> ImportCAFileSelected -> starts async read
  //  3. read completes -> ImportCAFileRead -> parse certs -> Certificate trust
  //  level dialog is shown.
  //  4. user clicks ok -> HandleImportCATrustSelected -> attempt import
  //  5a. if import succeeds -> ImportExportCleanup
  //  5b. if import fails -> show error, ImportExportCleanup
  void HandleImportCA(const base::ListValue* args);
  void ImportCAFileSelected(const base::FilePath& path);
  void ImportCAFileRead(const int* read_errno, const std::string* data);
  void HandleImportCATrustSelected(const base::ListValue* args);

  // Export a certificate.
  void HandleExportCertificate(const base::ListValue* args);

  // Delete certificate and private key (if any).
  void HandleDeleteCertificate(const base::ListValue* args);

  // Model initialization methods.
  void OnCertificateManagerModelCreated(
      std::unique_ptr<CertificateManagerModel> model);
  void CertificateManagerModelReady();

  // Populate the trees in all the tabs.
  void HandleRefreshCertificates(const base::ListValue* args);

  // Populate the given tab's tree.
  void PopulateTree(const std::string& tab_name, net::CertType type);

  void ResolveCallback(const base::Value& response);
  void RejectCallback(const base::Value& response);

  // Reject the pending JS callback with a generic error.
  void RejectCallbackWithError(const std::string& title,
                               const std::string& error);

  // Reject the pending JS callback with a certificate import error.
  void RejectCallbackWithImportError(
      const std::string& title,
      const net::NSSCertDatabase::ImportCertFailureList& not_imported);

  // Assigns a new |webui_callback_id_|. Ensures that previous in-flight request
  // has been fulfilled.
  void AssignWebUICallbackId(const base::ListValue* args);

  gfx::NativeWindow GetParentWindow() const;

  // If |args| is a list, parses the list element at |arg_index| as an id for
  // |cert_info_id_map_| and looks up the corresponding CertInfo. If there is
  // an error parsing the list, returns nullptr.
  CertificateManagerModel::CertInfo* GetCertInfoFromCallbackArgs(
      const base::Value& args,
      size_t arg_index);

#if defined(OS_CHROMEOS)
  // Returns true if the user may manage certificates on |slot| according
  // to ClientCertificateManagementAllowed policy.
  bool IsClientCertificateManagementAllowedPolicy(Slot slot) const;

  // Returns true if the user may manage certificates according
  // to CACertificateManagementAllowed policy.
  bool IsCACertificateManagementAllowedPolicy(CertificateSource source) const;
#endif  // defined(OS_CHROMEOS)

  // Returns true if the certificate represented by |cert_info| can be deleted.
  bool CanDeleteCertificate(
      const CertificateManagerModel::CertInfo* cert_info) const;

  // Returns true if the certificate represented by |cert_info| can be edited.
  bool CanEditCertificate(
      const CertificateManagerModel::CertInfo* cert_info) const;

  // The Certificates Manager model
  bool requested_certificate_manager_model_;
  std::unique_ptr<CertificateManagerModel> certificate_manager_model_;

  // For multi-step import or export processes, we need to store the path,
  // password, etc the user chose while we wait for them to enter a password,
  // wait for file to be read, etc.
  base::FilePath file_path_;
  base::string16 password_;
  // The WebUI callback ID of the last in-flight async request. There is always
  // only one in-flight such request.
  std::string webui_callback_id_;
  bool use_hardware_backed_;
  std::string file_data_;
  net::ScopedCERTCertificateList selected_cert_list_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  crypto::ScopedPK11Slot slot_;

  // Used in reading and writing certificate files.
  base::CancelableTaskTracker tracker_;
  scoped_refptr<FileAccessProvider> file_access_provider_;

  base::IDMap<std::unique_ptr<CertificateManagerModel::CertInfo>>
      cert_info_id_map_;

  base::WeakPtrFactory<CertificatesHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CertificatesHandler);
  friend class ::CertificateHandlerTest;
};

}  // namespace certificate_manager

#endif  // CHROME_BROWSER_UI_WEBUI_CERTIFICATES_HANDLER_H_
