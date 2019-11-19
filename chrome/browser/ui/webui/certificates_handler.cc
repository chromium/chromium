// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificates_handler.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"  // for FileAccessProvider
#include "base/i18n/string_compare.h"
#include "base/macros.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "ui/base/l10n/l10n_util.h"

using base::UTF8ToUTF16;

namespace {

// Field names for communicating certificate info to JS.
static const char kCertificatesHandlerEmailField[] = "email";
static const char kCertificatesHandlerExtractableField[] = "extractable";
static const char kCertificatesHandlerKeyField[] = "id";
static const char kCertificatesHandlerNameField[] = "name";
static const char kCertificatesHandlerObjSignField[] = "objSign";
static const char kCertificatesHandlerPolicyInstalledField[] = "policy";
static const char kCertificatesHandlerWebTrustAnchorField[] = "webTrustAnchor";
static const char kCertificatesHandlerCanBeDeletedField[] = "canBeDeleted";
static const char kCertificatesHandlerCanBeEditedField[] = "canBeEdited";
static const char kCertificatesHandlerSslField[] = "ssl";
static const char kCertificatesHandlerSubnodesField[] = "subnodes";
static const char kCertificatesHandlerContainsPolicyCertsField[] =
    "containsPolicyCerts";
static const char kCertificatesHandlerUntrustedField[] = "untrusted";

// Field names for communicating erros to JS.
static const char kCertificatesHandlerCertificateErrors[] = "certificateErrors";
static const char kCertificatesHandlerErrorDescription[] = "description";
static const char kCertificatesHandlerErrorField[] = "error";
static const char kCertificatesHandlerErrorTitle[] = "title";

// Enumeration of different callers of SelectFile.  (Start counting at 1 so
// if SelectFile is accidentally called with params=nullptr it won't match any.)
enum {
  EXPORT_PERSONAL_FILE_SELECTED = 1,
  IMPORT_PERSONAL_FILE_SELECTED,
  IMPORT_SERVER_FILE_SELECTED,
  IMPORT_CA_FILE_SELECTED,
};

std::string OrgNameToId(const std::string& org) {
  return "org-" + org;
}

struct DictionaryIdComparator {
  explicit DictionaryIdComparator(icu::Collator* collator)
      : collator_(collator) {}

  bool operator()(const base::Value& a, const base::Value& b) const {
    DCHECK(a.type() == base::Value::Type::DICTIONARY);
    DCHECK(b.type() == base::Value::Type::DICTIONARY);
    const base::DictionaryValue* a_dict;
    bool a_is_dictionary = a.GetAsDictionary(&a_dict);
    DCHECK(a_is_dictionary);
    const base::DictionaryValue* b_dict;
    bool b_is_dictionary = b.GetAsDictionary(&b_dict);
    DCHECK(b_is_dictionary);
    base::string16 a_str;
    base::string16 b_str;
    a_dict->GetString(kCertificatesHandlerNameField, &a_str);
    b_dict->GetString(kCertificatesHandlerNameField, &b_str);
    if (collator_ == nullptr)
      return a_str < b_str;
    return base::i18n::CompareString16WithCollator(*collator_, a_str, b_str) ==
           UCOL_LESS;
  }

  icu::Collator* collator_;
};

std::string NetErrorToString(int net_error) {
  switch (net_error) {
    // TODO(mattm): handle more cases.
    case net::ERR_IMPORT_CA_CERT_NOT_CA:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_NOT_CA);
    case net::ERR_IMPORT_CERT_ALREADY_EXISTS:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_CERT_ALREADY_EXISTS);
    default:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR);
  }
}

// Struct to bind the Equals member function to an object for use in find_if.
struct CertEquals {
  explicit CertEquals(CERTCertificate* cert) : cert_(cert) {}
  bool operator()(const scoped_refptr<net::X509Certificate> cert) const {
    return net::x509_util::IsSameCertificate(cert_, cert.get());
  }
  CERTCertificate* cert_;
};

// Determine if |data| could be a PFX Protocol Data Unit.
// This only does the minimum parsing necessary to distinguish a PFX file from a
// DER encoded Certificate.
//
// From RFC 7292 section 4:
//   PFX ::= SEQUENCE {
//       version     INTEGER {v3(3)}(v3,...),
//       authSafe    ContentInfo,
//       macData     MacData OPTIONAL
//   }
// From RFC 5280 section 4.1:
//   Certificate  ::=  SEQUENCE  {
//       tbsCertificate       TBSCertificate,
//       signatureAlgorithm   AlgorithmIdentifier,
//       signatureValue       BIT STRING  }
//
//  Certificate must be DER encoded, while PFX may be BER encoded.
//  Therefore PFX can be distingushed by checking if the file starts with an
//  indefinite SEQUENCE, or a definite SEQUENCE { INTEGER,  ... }.
bool CouldBePFX(const std::string& data) {
  if (data.size() < 4)
    return false;

  // Indefinite length SEQUENCE.
  if (data[0] == 0x30 && static_cast<uint8_t>(data[1]) == 0x80)
    return true;

  // If the SEQUENCE is definite length, it can be parsed through the version
  // tag using DER parser, since INTEGER must be definite length, even in BER.
  net::der::Parser parser((net::der::Input(&data)));
  net::der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;
  if (!sequence_parser.SkipTag(net::der::kInteger))
    return false;
  return true;
}

}  // namespace

namespace certificate_manager {

///////////////////////////////////////////////////////////////////////////////
//  FileAccessProvider

// TODO(mattm): Move to some shared location?
class FileAccessProvider
    : public base::RefCountedThreadSafe<FileAccessProvider> {
 public:
  // The first parameter is 0 on success or errno on failure. The second
  // parameter is read result.
  typedef base::Callback<void(const int*, const std::string*)> ReadCallback;

  // The first parameter is 0 on success or errno on failure. The second
  // parameter is the number of bytes written on success.
  typedef base::Callback<void(const int*, const int*)> WriteCallback;

  base::CancelableTaskTracker::TaskId StartRead(
      const base::FilePath& path,
      const ReadCallback& callback,
      base::CancelableTaskTracker* tracker);
  base::CancelableTaskTracker::TaskId StartWrite(
      const base::FilePath& path,
      const std::string& data,
      const WriteCallback& callback,
      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<FileAccessProvider>;
  virtual ~FileAccessProvider() {}

  // Reads file at |path|. |saved_errno| is 0 on success or errno on failure.
  // When success, |data| has file content.
  void DoRead(const base::FilePath& path, int* saved_errno, std::string* data);
  // Writes data to file at |path|. |saved_errno| is 0 on success or errno on
  // failure. When success, |bytes_written| has number of bytes written.
  void DoWrite(const base::FilePath& path,
               const std::string& data,
               int* saved_errno,
               int* bytes_written);
};

base::CancelableTaskTracker::TaskId FileAccessProvider::StartRead(
    const base::FilePath& path,
    const ReadCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  std::string* data = new std::string();

  // Post task to a background sequence to read file.
  auto task_runner = base::CreateTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  return tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoRead, this, path, saved_errno,
                     data),
      base::BindOnce(callback, base::Owned(saved_errno), base::Owned(data)));
}

base::CancelableTaskTracker::TaskId FileAccessProvider::StartWrite(
    const base::FilePath& path,
    const std::string& data,
    const WriteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  int* bytes_written = new int(0);

  // This task blocks shutdown because it saves critical user data.
  auto task_runner = base::CreateTaskRunner(
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  return tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoWrite, this, path, data,
                     saved_errno, bytes_written),
      base::BindOnce(callback, base::Owned(saved_errno),
                     base::Owned(bytes_written)));
}

void FileAccessProvider::DoRead(const base::FilePath& path,
                                int* saved_errno,
                                std::string* data) {
  bool success = base::ReadFileToString(path, data);
  *saved_errno = success ? 0 : errno;
}

void FileAccessProvider::DoWrite(const base::FilePath& path,
                                 const std::string& data,
                                 int* saved_errno,
                                 int* bytes_written) {
  *bytes_written = base::WriteFile(path, data.data(), data.size());
  *saved_errno = *bytes_written >= 0 ? 0 : errno;
}

///////////////////////////////////////////////////////////////////////////////
//  CertificatesHandler

CertificatesHandler::CertificatesHandler()
    : requested_certificate_manager_model_(false),
      use_hardware_backed_(false),
      file_access_provider_(base::MakeRefCounted<FileAccessProvider>()) {}

CertificatesHandler::~CertificatesHandler() {}

void CertificatesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "viewCertificate",
      base::BindRepeating(&CertificatesHandler::HandleViewCertificate,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getCaCertificateTrust",
      base::BindRepeating(&CertificatesHandler::HandleGetCATrust,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "editCaCertificateTrust",
      base::BindRepeating(&CertificatesHandler::HandleEditCATrust,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "cancelImportExportCertificate",
      base::BindRepeating(&CertificatesHandler::HandleCancelImportExportProcess,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificate",
      base::BindRepeating(&CertificatesHandler::HandleExportPersonal,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificatePasswordSelected",
      base::BindRepeating(
          &CertificatesHandler::HandleExportPersonalPasswordSelected,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importPersonalCertificate",
      base::BindRepeating(&CertificatesHandler::HandleImportPersonal,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importPersonalCertificatePasswordSelected",
      base::BindRepeating(
          &CertificatesHandler::HandleImportPersonalPasswordSelected,
          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importCaCertificate",
      base::BindRepeating(&CertificatesHandler::HandleImportCA,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importCaCertificateTrustSelected",
      base::BindRepeating(&CertificatesHandler::HandleImportCATrustSelected,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importServerCertificate",
      base::BindRepeating(&CertificatesHandler::HandleImportServer,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportCertificate",
      base::BindRepeating(&CertificatesHandler::HandleExportCertificate,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "deleteCertificate",
      base::BindRepeating(&CertificatesHandler::HandleDeleteCertificate,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "refreshCertificates",
      base::BindRepeating(&CertificatesHandler::HandleRefreshCertificates,
                          base::Unretained(this)));
}

void CertificatesHandler::CertificatesRefreshed() {
  PopulateTree("personalCerts", net::USER_CERT);
  PopulateTree("serverCerts", net::SERVER_CERT);
  PopulateTree("caCerts", net::CA_CERT);
  PopulateTree("otherCerts", net::OTHER_CERT);
}

void CertificatesHandler::FileSelected(const base::FilePath& path,
                                       int index,
                                       void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
      ExportPersonalFileSelected(path);
      break;
    case IMPORT_PERSONAL_FILE_SELECTED:
      ImportPersonalFileSelected(path);
      break;
    case IMPORT_SERVER_FILE_SELECTED:
      ImportServerFileSelected(path);
      break;
    case IMPORT_CA_FILE_SELECTED:
      ImportCAFileSelected(path);
      break;
    default:
      NOTREACHED();
  }
}

void CertificatesHandler::FileSelectionCanceled(void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_SERVER_FILE_SELECTED:
    case IMPORT_CA_FILE_SELECTED:
      ImportExportCleanup();
      RejectCallback(base::Value());
      break;
    default:
      NOTREACHED();
  }
}

void CertificatesHandler::HandleViewCertificate(const base::ListValue* args) {
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 0 /* arg_index */);
  if (!cert_info)
    return;
  net::ScopedCERTCertificateList certs;
  certs.push_back(net::x509_util::DupCERTCertificate(cert_info->cert()));
  CertificateViewerDialog::ShowConstrained(
      std::move(certs), web_ui()->GetWebContents(), GetParentWindow());
}

void CertificatesHandler::AssignWebUICallbackId(const base::ListValue* args) {
  CHECK_LE(1U, args->GetSize());
  CHECK(webui_callback_id_.empty());
  CHECK(args->GetString(0, &webui_callback_id_));
}

void CertificatesHandler::HandleGetCATrust(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 1 /* arg_index */);
  if (!cert_info)
    return;

  net::NSSCertDatabase::TrustBits trust_bits =
      certificate_manager_model_->cert_db()->GetCertTrust(cert_info->cert(),
                                                          net::CA_CERT);
  std::unique_ptr<base::DictionaryValue> ca_trust_info(
      new base::DictionaryValue);
  ca_trust_info->SetBoolean(
      kCertificatesHandlerSslField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_SSL));
  ca_trust_info->SetBoolean(
      kCertificatesHandlerEmailField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_EMAIL));
  ca_trust_info->SetBoolean(
      kCertificatesHandlerObjSignField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_OBJ_SIGN));
  ResolveCallback(*ca_trust_info);
}

void CertificatesHandler::HandleEditCATrust(const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  AssignWebUICallbackId(args);

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 1 /* arg_index */);
  if (!cert_info)
    return;

  if (!CanEditCertificate(cert_info)) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SET_TRUST_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_NOT_ALLOWED));
    return;
  }

  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  CHECK(args->GetBoolean(2, &trust_ssl));
  CHECK(args->GetBoolean(3, &trust_email));
  CHECK(args->GetBoolean(4, &trust_obj_sign));

  bool result = certificate_manager_model_->SetCertTrust(
      cert_info->cert(), net::CA_CERT,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN);
  if (!result) {
    // TODO(mattm): better error messages?
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SET_TRUST_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::HandleExportPersonal(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 1 /* arg_index */);
  if (!cert_info)
    return;

  selected_cert_list_.push_back(
      net::x509_util::DupCERTCertificate(cert_info->cert()));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(EXPORT_PERSONAL_FILE_SELECTED));
}

void CertificatesHandler::ExportPersonalFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  ResolveCallback(base::Value());
}

void CertificatesHandler::HandleExportPersonalPasswordSelected(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetString(1, &password_));

  // Currently, we don't support exporting more than one at a time.  If we do,
  // this would need to either change this to use UnlockSlotsIfNecessary or
  // change UnlockCertSlotIfNecessary to take a CertificateList.
  DCHECK_EQ(selected_cert_list_.size(), 1U);

  // TODO(mattm): do something smarter about non-extractable keys
  chrome::UnlockCertSlotIfNecessary(
      selected_cert_list_[0].get(), kCryptoModulePasswordCertExport,
      net::HostPortPair(),  // unused.
      GetParentWindow(),
      base::BindOnce(&CertificatesHandler::ExportPersonalSlotsUnlocked,
                     base::Unretained(this)));
}

void CertificatesHandler::ExportPersonalSlotsUnlocked() {
  std::string output;
  int num_exported = certificate_manager_model_->cert_db()->ExportToPKCS12(
      selected_cert_list_, password_, &output);
  if (!num_exported) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartWrite(
      file_path_, output,
      base::Bind(&CertificatesHandler::ExportPersonalFileWritten,
                 base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ExportPersonalFileWritten(const int* write_errno,
                                                    const int* bytes_written) {
  ImportExportCleanup();
  if (*write_errno) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_WRITE_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*write_errno))));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::HandleImportPersonal(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  // When policy changes while user on the certificate manager page, the UI
  // doesn't update without page refresh and user can still see and use import
  // button. Because of this 'return' the button will do nothing.
  if (!IsClientCertificateManagementAllowedPolicy(Slot::kUser)) {
    return;
  }
#endif

  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetBoolean(1, &use_hardware_backed_));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pfx"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_USAGE_SSL_CLIENT));
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_PERSONAL_FILE_SELECTED));
}

void CertificatesHandler::ImportPersonalFileSelected(
    const base::FilePath& path) {
  file_access_provider_->StartRead(
      path,
      base::Bind(&CertificatesHandler::ImportPersonalFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportPersonalFileRead(const int* read_errno,
                                                 const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  file_data_ = *data;

  if (CouldBePFX(file_data_)) {
    ResolveCallback(base::Value(true));
    return;
  }

  // Non .p12/.pfx files are assumed to be single/chain certificates without
  // private key data. The default extension according to spec is '.crt',
  // however other extensions are also used in some places to represent these
  // certificates.
  int result = certificate_manager_model_->ImportUserCert(file_data_);
  ImportExportCleanup();
  int string_id;
  switch (result) {
    case net::OK:
      ResolveCallback(base::Value(false));
      return;
    case net::ERR_NO_PRIVATE_KEY_FOR_CERT:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_MISSING_KEY;
      break;
    case net::ERR_CERT_INVALID:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_FILE;
      break;
    default:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR;
      break;
  }
  RejectCallbackWithError(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
      l10n_util::GetStringUTF8(string_id));
}

void CertificatesHandler::HandleImportPersonalPasswordSelected(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetString(1, &password_));

  if (use_hardware_backed_) {
    slot_ = certificate_manager_model_->cert_db()->GetPrivateSlot();
  } else {
    slot_ = certificate_manager_model_->cert_db()->GetPublicSlot();
  }

  std::vector<crypto::ScopedPK11Slot> modules;
  modules.push_back(crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())));
  chrome::UnlockSlotsIfNecessary(
      std::move(modules), kCryptoModulePasswordCertImport,
      net::HostPortPair(),  // unused.
      GetParentWindow(),
      base::BindOnce(&CertificatesHandler::ImportPersonalSlotUnlocked,
                     base::Unretained(this)));
}

void CertificatesHandler::ImportPersonalSlotUnlocked() {
  // Determine if the private key should be unextractable after the import.
  // We do this by checking the value of |use_hardware_backed_| which is set
  // to true if importing into a hardware module. Currently, this only happens
  // for Chrome OS when the "Import and Bind" option is chosen.
  bool is_extractable = !use_hardware_backed_;
  int result = certificate_manager_model_->ImportFromPKCS12(
      slot_.get(), file_data_, password_, is_extractable);
  ImportExportCleanup();
  int string_id;
  switch (result) {
    case net::OK:
      ResolveCallback(base::Value());
      return;
    case net::ERR_PKCS12_IMPORT_BAD_PASSWORD:
      // TODO(mattm): if the error was a bad password, we should reshow the
      // password dialog after the user dismisses the error dialog.
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_BAD_PASSWORD;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_MAC:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_MAC;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_FILE:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_FILE;
      break;
    case net::ERR_PKCS12_IMPORT_UNSUPPORTED:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_UNSUPPORTED;
      break;
    default:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR;
      break;
  }
  RejectCallbackWithError(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
      l10n_util::GetStringUTF8(string_id));
}

void CertificatesHandler::HandleCancelImportExportProcess(
    const base::ListValue* args) {
  ImportExportCleanup();
}

void CertificatesHandler::ImportExportCleanup() {
  file_path_.clear();
  password_.clear();
  file_data_.clear();
  use_hardware_backed_ = false;
  selected_cert_list_.clear();
  slot_.reset();
  tracker_.TryCancelAll();

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
  select_file_dialog_ = nullptr;
}

void CertificatesHandler::HandleImportServer(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  AssignWebUICallbackId(args);

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(
      select_file_dialog_.get(), ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::FilePath(), GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_SERVER_FILE_SELECTED));
}

void CertificatesHandler::ImportServerFileSelected(const base::FilePath& path) {
  file_access_provider_->StartRead(
      path,
      base::Bind(&CertificatesHandler::ImportServerFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportServerFileRead(const int* read_errno,
                                               const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::x509_util::CreateCERTCertificateListFromBytes(
      data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  net::NSSCertDatabase::ImportCertFailureList not_imported;
  // TODO(mattm): Add UI for trust. http://crbug.com/76274
  bool result = certificate_manager_model_->ImportServerCert(
      selected_cert_list_, net::NSSCertDatabase::TRUST_DEFAULT, &not_imported);
  if (!result) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    RejectCallbackWithImportError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        not_imported);
  } else {
    ResolveCallback(base::Value());
  }
  ImportExportCleanup();
}

void CertificatesHandler::HandleImportCA(const base::ListValue* args) {
#if defined(OS_CHROMEOS)
  // When policy changes while user on the certificate manager page, the UI
  // doesn't update without page refresh and user can still see and use import
  // button. Because of this 'return' the button will do nothing.
  if (!IsCACertificateManagementAllowedPolicy(CertificateSource::kImported)) {
    return;
  }
#endif  // defined(OS_CHROMEOS)

  CHECK_EQ(1U, args->GetSize());
  AssignWebUICallbackId(args);

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_OPEN_FILE,
                           base::FilePath(), GetParentWindow(),
                           reinterpret_cast<void*>(IMPORT_CA_FILE_SELECTED));
}

void CertificatesHandler::ImportCAFileSelected(const base::FilePath& path) {
  file_access_provider_->StartRead(
      path,
      base::Bind(&CertificatesHandler::ImportCAFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportCAFileRead(const int* read_errno,
                                           const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::x509_util::CreateCERTCertificateListFromBytes(
      data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  CERTCertificate* root_cert =
      certificate_manager_model_->cert_db()->FindRootInList(
          selected_cert_list_);

  // TODO(mattm): check here if root_cert is not a CA cert and show error.

  base::Value cert_name(
      x509_certificate_model::GetSubjectDisplayName(root_cert));
  ResolveCallback(cert_name);
}

void CertificatesHandler::HandleImportCATrustSelected(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  AssignWebUICallbackId(args);

  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  CHECK(args->GetBoolean(1, &trust_ssl));
  CHECK(args->GetBoolean(2, &trust_email));
  CHECK(args->GetBoolean(3, &trust_obj_sign));

  // TODO(mattm): add UI for setting explicit distrust, too.
  // http://crbug.com/128411
  net::NSSCertDatabase::ImportCertFailureList not_imported;
  bool result = certificate_manager_model_->ImportCACerts(
      selected_cert_list_,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN,
      &not_imported);
  if (!result) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    RejectCallbackWithImportError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        not_imported);
  } else {
    ResolveCallback(base::Value());
  }
  ImportExportCleanup();
}

void CertificatesHandler::HandleExportCertificate(const base::ListValue* args) {
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 0 /* arg_index */);
  if (!cert_info)
    return;

  net::ScopedCERTCertificateList export_certs;
  export_certs.push_back(net::x509_util::DupCERTCertificate(cert_info->cert()));
  ShowCertExportDialog(web_ui()->GetWebContents(), GetParentWindow(),
                       export_certs.begin(), export_certs.end());
}

void CertificatesHandler::HandleDeleteCertificate(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(*args, 1 /* arg_index */);
  if (!cert_info)
    return;

  if (!CanDeleteCertificate(cert_info)) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CERT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_NOT_ALLOWED));
    return;
  }

  bool result = certificate_manager_model_->Delete(cert_info->cert());
  if (!result) {
    // TODO(mattm): better error messages?
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CERT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::OnCertificateManagerModelCreated(
    std::unique_ptr<CertificateManagerModel> model) {
  certificate_manager_model_ = std::move(model);
  CertificateManagerModelReady();
}

void CertificatesHandler::CertificateManagerModelReady() {
  bool client_import_allowed = true;
  bool ca_import_allowed = true;
#if defined(OS_CHROMEOS)
  client_import_allowed =
      IsClientCertificateManagementAllowedPolicy(Slot::kUser);
  ca_import_allowed =
      IsCACertificateManagementAllowedPolicy(CertificateSource::kImported);
#endif  // defined(OS_CHROMEOS)
  if (IsJavascriptAllowed()) {
    FireWebUIListener("client-import-allowed-changed",
                      base::Value(client_import_allowed));
    FireWebUIListener("ca-import-allowed-changed",
                      base::Value(ca_import_allowed));
  }
  certificate_manager_model_->Refresh();
}

void CertificatesHandler::HandleRefreshCertificates(
    const base::ListValue* args) {
  AllowJavascript();

  if (certificate_manager_model_) {
    // Already have a model, the webui must be re-loading.  Just re-run the
    // webui initialization.
    CertificateManagerModelReady();
    return;
  }

  if (!requested_certificate_manager_model_) {
    // Request that a model be created.
    CertificateManagerModel::Create(
        Profile::FromWebUI(web_ui()), this,
        base::Bind(&CertificatesHandler::OnCertificateManagerModelCreated,
                   weak_ptr_factory_.GetWeakPtr()));
    requested_certificate_manager_model_ = true;
    return;
  }

  // We are already waiting for a CertificateManagerModel to be created, no need
  // to do anything.
}

void CertificatesHandler::PopulateTree(const std::string& tab_name,
                                       net::CertType type) {
  std::unique_ptr<icu::Collator> collator;
  UErrorCode error = U_ZERO_ERROR;
  collator.reset(icu::Collator::createInstance(
      icu::Locale(g_browser_process->GetApplicationLocale().c_str()), error));
  if (U_FAILURE(error))
    collator.reset();
  DictionaryIdComparator comparator(collator.get());
  CertificateManagerModel::OrgGroupingMap org_grouping_map;

  certificate_manager_model_->FilterAndBuildOrgGroupingMap(type,
                                                           &org_grouping_map);

  base::ListValue nodes;
  for (auto& org_grouping_map_entry : org_grouping_map) {
    // Populate first level (org name).
    base::DictionaryValue org_dict;
    org_dict.SetKey(kCertificatesHandlerKeyField,
                    base::Value(OrgNameToId(org_grouping_map_entry.first)));
    org_dict.SetKey(kCertificatesHandlerNameField,
                    base::Value(org_grouping_map_entry.first));

    // Populate second level (certs).
    base::ListValue subnodes;
    bool contains_policy_certs = false;
    for (auto& org_cert : org_grouping_map_entry.second) {
      // Move the CertInfo into |cert_info_id_map_|.
      CertificateManagerModel::CertInfo* cert_info = org_cert.get();
      std::string id =
          base::NumberToString(cert_info_id_map_.Add(std::move(org_cert)));

      base::DictionaryValue cert_dict;
      cert_dict.SetKey(kCertificatesHandlerKeyField, base::Value(id));
      cert_dict.SetKey(kCertificatesHandlerNameField,
                       base::Value(cert_info->name()));
      cert_dict.SetKey(kCertificatesHandlerCanBeDeletedField,
                       base::Value(CanDeleteCertificate(cert_info)));
      cert_dict.SetKey(kCertificatesHandlerCanBeEditedField,
                       base::Value(CanEditCertificate(cert_info)));
      cert_dict.SetKey(kCertificatesHandlerUntrustedField,
                       base::Value(cert_info->untrusted()));
      cert_dict.SetKey(
          kCertificatesHandlerPolicyInstalledField,
          base::Value(cert_info->source() ==
                      CertificateManagerModel::CertInfo::Source::kPolicy));
      cert_dict.SetKey(kCertificatesHandlerWebTrustAnchorField,
                       base::Value(cert_info->web_trust_anchor()));
      // TODO(hshi): This should be determined by testing for PKCS #11
      // CKA_EXTRACTABLE attribute. We may need to use the NSS function
      // PK11_ReadRawAttribute to do that.
      cert_dict.SetKey(kCertificatesHandlerExtractableField,
                       base::Value(!cert_info->hardware_backed()));
      // TODO(mattm): Other columns.
      subnodes.Append(std::move(cert_dict));

      contains_policy_certs |=
          cert_info->source() ==
          CertificateManagerModel::CertInfo::Source::kPolicy;
    }
    std::sort(subnodes.GetList().begin(), subnodes.GetList().end(), comparator);

    org_dict.SetKey(kCertificatesHandlerContainsPolicyCertsField,
                    base::Value(contains_policy_certs));
    org_dict.SetKey(kCertificatesHandlerSubnodesField, std::move(subnodes));
    nodes.Append(std::move(org_dict));
  }
  std::sort(nodes.GetList().begin(), nodes.GetList().end(), comparator);

  if (IsJavascriptAllowed()) {
    FireWebUIListener("certificates-changed", base::Value(tab_name),
                      std::move(nodes));
  }
}

void CertificatesHandler::ResolveCallback(const base::Value& response) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallback(const base::Value& response) {
  DCHECK(!webui_callback_id_.empty());
  RejectJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallbackWithError(const std::string& title,
                                                  const std::string& error) {
  std::unique_ptr<base::DictionaryValue> error_info(new base::DictionaryValue);
  error_info->SetString(kCertificatesHandlerErrorTitle, title);
  error_info->SetString(kCertificatesHandlerErrorDescription, error);
  RejectCallback(*error_info);
}

void CertificatesHandler::RejectCallbackWithImportError(
    const std::string& title,
    const net::NSSCertDatabase::ImportCertFailureList& not_imported) {
  std::string error;
  if (selected_cert_list_.size() == 1)
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_SINGLE_NOT_IMPORTED);
  else if (not_imported.size() == selected_cert_list_.size())
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ALL_NOT_IMPORTED);
  else
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_SOME_NOT_IMPORTED);

  std::unique_ptr<base::ListValue> cert_error_list =
      std::make_unique<base::ListValue>();
  for (size_t i = 0; i < not_imported.size(); ++i) {
    const net::NSSCertDatabase::ImportCertFailure& failure = not_imported[i];
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetString(kCertificatesHandlerNameField,
                    x509_certificate_model::GetSubjectDisplayName(
                        failure.certificate.get()));
    dict->SetString(kCertificatesHandlerErrorField,
                    NetErrorToString(failure.net_error));
    cert_error_list->Append(std::move(dict));
  }

  std::unique_ptr<base::DictionaryValue> error_info(new base::DictionaryValue);
  error_info->SetString(kCertificatesHandlerErrorTitle, title);
  error_info->SetString(kCertificatesHandlerErrorDescription, error);
  error_info->Set(kCertificatesHandlerCertificateErrors,
                  std::move(cert_error_list));
  RejectCallback(*error_info);
}

gfx::NativeWindow CertificatesHandler::GetParentWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

CertificateManagerModel::CertInfo*
CertificatesHandler::GetCertInfoFromCallbackArgs(const base::Value& args,
                                                 size_t arg_index) {
  if (!args.is_list())
    return nullptr;
  if (arg_index >= args.GetList().size())
    return nullptr;
  const auto& arg = args.GetList()[arg_index];
  if (!arg.is_string())
    return nullptr;

  int32_t cert_info_id = 0;
  if (!base::StringToInt(arg.GetString(), &cert_info_id))
    return nullptr;

  return cert_info_id_map_.Lookup(cert_info_id);
}

#if defined(OS_CHROMEOS)
bool CertificatesHandler::IsClientCertificateManagementAllowedPolicy(
    Slot slot) const {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  auto policy_value = static_cast<ClientCertificateManagementPermission>(
      prefs->GetInteger(prefs::kClientCertificateManagementAllowed));

  if (slot == Slot::kUser) {
    return policy_value != ClientCertificateManagementPermission::kNone;
  }
  return policy_value == ClientCertificateManagementPermission::kAll;
}

bool CertificatesHandler::IsCACertificateManagementAllowedPolicy(
    CertificateSource source) const {
  Profile* profile = Profile::FromWebUI(web_ui());
  PrefService* prefs = profile->GetPrefs();
  auto policy_value = static_cast<CACertificateManagementPermission>(
      prefs->GetInteger(prefs::kCACertificateManagementAllowed));

  switch (source) {
    case CertificateSource::kBuiltIn:
      return policy_value == CACertificateManagementPermission::kAll;
    case CertificateSource::kImported:
      return policy_value != CACertificateManagementPermission::kNone;
  }
}
#endif  // defined(OS_CHROMEOS)

bool CertificatesHandler::CanDeleteCertificate(
    const CertificateManagerModel::CertInfo* cert_info) const {
  if (!cert_info->can_be_deleted() ||
      cert_info->source() ==
          CertificateManagerModel::CertInfo::Source::kPolicy) {
    return false;
  }

#if defined(OS_CHROMEOS)
  if (cert_info->type() == net::CertType::USER_CERT) {
    return IsClientCertificateManagementAllowedPolicy(
        cert_info->device_wide() ? Slot::kSystem : Slot::kUser);
  }
  if (cert_info->type() == net::CertType::CA_CERT) {
    CertificateSource source = cert_info->can_be_deleted()
                                   ? CertificateSource::kImported
                                   : CertificateSource::kBuiltIn;
    return IsCACertificateManagementAllowedPolicy(source);
  }
#endif  // defined(OS_CHROMEOS)
  return true;
}

bool CertificatesHandler::CanEditCertificate(
    const CertificateManagerModel::CertInfo* cert_info) const {
  if ((cert_info->type() != net::CertType::CA_CERT) ||
      (cert_info->source() ==
       CertificateManagerModel::CertInfo::Source::kPolicy)) {
    return false;
  }
#if defined(OS_CHROMEOS)
  CertificateSource source = cert_info->can_be_deleted()
                                 ? CertificateSource::kImported
                                 : CertificateSource::kBuiltIn;
  return IsCACertificateManagementAllowedPolicy(source);
#endif  // defined(OS_CHROMEOS)
  return true;
}

#if defined(OS_CHROMEOS)
void CertificatesHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Allow users to manage all client certificates by default. This can be
  // overridden by enterprise policy.
  registry->RegisterIntegerPref(
      prefs::kClientCertificateManagementAllowed,
      static_cast<int>(ClientCertificateManagementPermission::kAll));

  // Allow users to manage all CA certificates by default. This can be
  // overridden by enterprise policy.
  registry->RegisterIntegerPref(
      prefs::kCACertificateManagementAllowed,
      static_cast<int>(CACertificateManagementPermission::kAll));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace certificate_manager
