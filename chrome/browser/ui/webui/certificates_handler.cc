// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/certificates_handler.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "base/files/file_util.h"  // for FileAccessProvider
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/string_compare.h"
#include "base/memory/raw_ptr.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/span.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/selected_file_info.h"

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

std::string OrgNameToId(const std::string& org) {
  return "org-" + org;
}

struct DictionaryIdComparator {
  explicit DictionaryIdComparator(icu::Collator* collator)
      : collator_(collator) {}

  bool operator()(const base::Value& a, const base::Value& b) const {
    DCHECK(a.type() == base::Value::Type::DICT);
    DCHECK(b.type() == base::Value::Type::DICT);
    const base::Value::Dict& a_dict = a.GetDict();
    const base::Value::Dict& b_dict = b.GetDict();
    std::u16string a_str;
    std::u16string b_str;
    const std::string* ptr = a_dict.FindString(kCertificatesHandlerNameField);
    if (ptr)
      a_str = base::UTF8ToUTF16(*ptr);
    ptr = b_dict.FindString(kCertificatesHandlerNameField);
    if (ptr)
      b_str = base::UTF8ToUTF16(*ptr);
    if (collator_ == nullptr)
      return a_str < b_str;
    return base::i18n::CompareString16WithCollator(*collator_, a_str, b_str) ==
           UCOL_LESS;
  }

  raw_ptr<icu::Collator> collator_;
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
  raw_ptr<CERTCertificate> cert_;
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
bool CouldBePFX(std::string_view data) {
  if (data.size() < 4)
    return false;

  // Indefinite length SEQUENCE.
  if (data[0] == 0x30 && static_cast<uint8_t>(data[1]) == 0x80)
    return true;

  // If the SEQUENCE is definite length, it can be parsed through the version
  // tag using DER parser, since INTEGER must be definite length, even in BER.
  CBS cbs = bssl::StringAsBytes(data);
  CBS sequence, version;
  return CBS_get_asn1(&cbs, &sequence, CBS_ASN1_SEQUENCE) &&
         CBS_get_asn1(&sequence, &version, CBS_ASN1_INTEGER);
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
  typedef base::OnceCallback<void(const int*, const std::string*)> ReadCallback;

  // The first parameter is 0 on success or errno on failure.
  typedef base::OnceCallback<void(const int*)> WriteCallback;

  base::CancelableTaskTracker::TaskId StartRead(
      const base::FilePath& path,
      ReadCallback callback,
      base::CancelableTaskTracker* tracker,
      file_access::ScopedFileAccess file_access);
  base::CancelableTaskTracker::TaskId StartWrite(
      const base::FilePath& path,
      const std::string& data,
      WriteCallback callback,
      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<FileAccessProvider>;
  virtual ~FileAccessProvider() {}

  // Reads file at |path|. |saved_errno| is 0 on success or errno on failure.
  // When success, |data| has file content.
  void DoRead(const base::FilePath& path,
              int* saved_errno,
              std::string* data,
              file_access::ScopedFileAccess file_access);
  // Writes data to file at |path|. |saved_errno| is 0 on success or errno on
  // failure.
  void DoWrite(const base::FilePath& path,
               const std::string& data,
               int* saved_errno);
};

base::CancelableTaskTracker::TaskId FileAccessProvider::StartRead(
    const base::FilePath& path,
    ReadCallback callback,
    base::CancelableTaskTracker* tracker,
    file_access::ScopedFileAccess file_access) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  std::string* data = new std::string();

  // Post task to a background sequence to read file.
  auto task_runner = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  return tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoRead, this, path, saved_errno, data,
                     std::move(file_access)),
      base::BindOnce(std::move(callback), base::Owned(saved_errno),
                     base::Owned(data)));
}

base::CancelableTaskTracker::TaskId FileAccessProvider::StartWrite(
    const base::FilePath& path,
    const std::string& data,
    WriteCallback callback,
    base::CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);

  // This task blocks shutdown because it saves critical user data.
  auto task_runner = base::ThreadPool::CreateTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
  return tracker->PostTaskAndReply(
      task_runner.get(), FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoWrite, this, path, data,
                     saved_errno),
      base::BindOnce(std::move(callback), base::Owned(saved_errno)));
}

// The `file_access` object for reading `path` should be in scope to
// successfully read the file when Data Leak Prevention policies are enabled.
void FileAccessProvider::DoRead(const base::FilePath& path,
                                int* saved_errno,
                                std::string* data,
                                file_access::ScopedFileAccess file_access) {
  bool success = base::ReadFileToString(path, data);
  *saved_errno = success ? 0 : errno;
}

void FileAccessProvider::DoWrite(const base::FilePath& path,
                                 const std::string& data,
                                 int* saved_errno) {
  if (base::WriteFile(path, data)) {
    *saved_errno = 0;
  } else {
    *saved_errno = errno;
  }
}

///////////////////////////////////////////////////////////////////////////////
//  CertificatesHandler

CertificatesHandler::CertificatesHandler()
    : requested_certificate_manager_model_(false),
      use_hardware_backed_(false),
      file_access_provider_(base::MakeRefCounted<FileAccessProvider>()) {}

CertificatesHandler::~CertificatesHandler() {
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
  select_file_dialog_.reset();
}

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
  if (ShouldDisplayClientCertificates()) {
    PopulateTree("personalCerts", net::USER_CERT);
  }
  PopulateTree("serverCerts", net::SERVER_CERT);
  PopulateTree("caCerts", net::CA_CERT);
  PopulateTree("otherCerts", net::OTHER_CERT);
}

void CertificatesHandler::FileSelected(const ui::SelectedFileInfo& file,
                                       int index) {
  CHECK(pending_operation_.has_value());
  switch (*pending_operation_) {
    case EXPORT_PERSONAL_FILE:
      ExportPersonalFileSelected(file.path());
      break;
    case IMPORT_PERSONAL_FILE:
      file_access::RequestFilesAccessForSystem(
          {file.path()},
          base::BindOnce(&CertificatesHandler::ImportPersonalFileSelected,
                         weak_ptr_factory_.GetWeakPtr(), file.path()));
      break;
    case IMPORT_SERVER_FILE:
      file_access::RequestFilesAccessForSystem(
          {file.path()},
          base::BindOnce(&CertificatesHandler::ImportServerFileSelected,
                         weak_ptr_factory_.GetWeakPtr(), file.path()));
      break;
    case IMPORT_CA_FILE:
      file_access::RequestFilesAccessForSystem(
          {file.path()},
          base::BindOnce(&CertificatesHandler::ImportCAFileSelected,
                         weak_ptr_factory_.GetWeakPtr(), file.path()));
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  select_file_dialog_.reset();
  pending_operation_ = std::nullopt;
}

void CertificatesHandler::FileSelectionCanceled() {
  ImportExportCleanup();
  RejectCallback(base::Value());
}

void CertificatesHandler::HandleViewCertificate(const base::Value::List& args) {
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 0 /* arg_index */);
  if (!cert_info)
    return;
  net::ScopedCERTCertificateList certs;
  certs.push_back(net::x509_util::DupCERTCertificate(cert_info->cert()));
  CertificateViewerDialog::ShowConstrained(
      std::move(certs), web_ui()->GetWebContents(), GetParentWindow());
}

bool CertificatesHandler::AssignWebUICallbackId(const base::Value::List& args) {
  CHECK_LE(1U, args.size());
  if (!webui_callback_id_.empty())
    return false;
  webui_callback_id_ = args[0].GetString();
  return true;
}

void CertificatesHandler::HandleGetCATrust(const base::Value::List& args) {
  AllowJavascript();

  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 1 /* arg_index */);
  if (!cert_info)
    return;

  net::NSSCertDatabase::TrustBits trust_bits =
      certificate_manager_model_->cert_db()->GetCertTrust(cert_info->cert(),
                                                          net::CA_CERT);

  ResolveCallback(
      base::Value::Dict()
          .Set(
              kCertificatesHandlerSslField,
              static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_SSL))
          .Set(kCertificatesHandlerEmailField,
               static_cast<bool>(trust_bits &
                                 net::NSSCertDatabase::TRUSTED_EMAIL))
          .Set(kCertificatesHandlerObjSignField,
               static_cast<bool>(trust_bits &
                                 net::NSSCertDatabase::TRUSTED_OBJ_SIGN)));
}

void CertificatesHandler::HandleEditCATrust(const base::Value::List& args) {
  CHECK_EQ(5U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 1 /* arg_index */);
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

  const bool trust_ssl = args[2].GetBool();
  const bool trust_email = args[3].GetBool();
  const bool trust_obj_sign = args[4].GetBool();

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

void CertificatesHandler::HandleExportPersonal(const base::Value::List& args) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 1 /* arg_index */);
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
  pending_operation_ = EXPORT_PERSONAL_FILE;
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                                  std::u16string(), base::FilePath(),
                                  &file_type_info, 1, FILE_PATH_LITERAL("p12"),
                                  GetParentWindow());
}

void CertificatesHandler::ExportPersonalFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  ResolveCallback(base::Value());
}

void CertificatesHandler::HandleExportPersonalPasswordSelected(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }
  password_ = UTF8ToUTF16(args[1].GetString());  // CHECKs if non-string.

  // Currently, we don't support exporting more than one at a time.  If we do,
  // this would need to either change this to use UnlockSlotsIfNecessary or
  // change UnlockCertSlotIfNecessary to take a CertificateList.
  CHECK_EQ(selected_cert_list_.size(), 1U);

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
      base::BindOnce(&CertificatesHandler::ExportPersonalFileWritten,
                     base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ExportPersonalFileWritten(const int* write_errno) {
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

void CertificatesHandler::HandleImportPersonal(const base::Value::List& args) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  // When the "allowed" value changes while user on the certificate manager
  // page, the UI doesn't update without page refresh and user can still see and
  // use import button. Because of this 'return' the button will do nothing.
  if (!IsClientCertificateManagementAllowed(Slot::kUser)) {
    return;
  }

  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }
  use_hardware_backed_ = args[1].GetBool();

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
  pending_operation_ = IMPORT_PERSONAL_FILE;
  select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                  std::u16string(), base::FilePath(),
                                  &file_type_info, 1, FILE_PATH_LITERAL("p12"),
                                  GetParentWindow());
}

void CertificatesHandler::ImportPersonalFileSelected(
    const base::FilePath& path,
    file_access::ScopedFileAccess file_access) {
  file_access_provider_->StartRead(
      path,
      base::BindOnce(&CertificatesHandler::ImportPersonalFileRead,
                     base::Unretained(this)),
      &tracker_, std::move(file_access));
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
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }
  password_ = UTF8ToUTF16(args[1].GetString());  // CHECKs if non-string.

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
  certificate_manager_model_->ImportFromPKCS12(
      slot_.get(), file_data_, password_, is_extractable,
      base::BindOnce(&CertificatesHandler::ImportPersonalResultReceived,
                     weak_ptr_factory_.GetWeakPtr()));
  ImportExportCleanup();
}

void CertificatesHandler::ImportPersonalResultReceived(int net_result) {
  int string_id;
  switch (net_result) {
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
    const base::Value::List& args) {
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
  select_file_dialog_.reset();
  pending_operation_ = std::nullopt;
}

void CertificatesHandler::HandleImportServer(const base::Value::List& args) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  CHECK_EQ(1U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  pending_operation_ = IMPORT_SERVER_FILE;
  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_OPEN_FILE,
                           base::FilePath(), GetParentWindow());
}

void CertificatesHandler::ImportServerFileSelected(
    const base::FilePath& path,
    file_access::ScopedFileAccess file_access) {
  file_access_provider_->StartRead(
      path,
      base::BindOnce(&CertificatesHandler::ImportServerFileRead,
                     base::Unretained(this)),
      &tracker_, std::move(file_access));
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
      base::as_byte_span(*data), net::X509Certificate::FORMAT_AUTO);
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

void CertificatesHandler::HandleImportCA(const base::Value::List& args) {
  // Early return if the select file dialog is already active.
  if (select_file_dialog_)
    return;

  // When the "allowed" value changes while user on the certificate manager
  // page, the UI doesn't update without page refresh and user can still see and
  // use import button. Because of this 'return' the button will do nothing.
  if (!IsCACertificateManagementAllowed(CertificateSource::kImported)) {
    return;
  }

  CHECK_EQ(1U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this,
      std::make_unique<ChromeSelectFilePolicy>(web_ui()->GetWebContents()));
  pending_operation_ = IMPORT_CA_FILE;
  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_OPEN_FILE,
                           base::FilePath(), GetParentWindow());
}

void CertificatesHandler::ImportCAFileSelected(
    const base::FilePath& path,
    file_access::ScopedFileAccess file_access) {
  file_access_provider_->StartRead(
      path,
      base::BindOnce(&CertificatesHandler::ImportCAFileRead,
                     base::Unretained(this)),
      &tracker_, std::move(file_access));
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
      base::as_byte_span(*data), net::X509Certificate::FORMAT_AUTO);
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
    const base::Value::List& args) {
  CHECK_EQ(4U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  const bool trust_ssl = args[1].GetBool();
  const bool trust_email = args[2].GetBool();
  const bool trust_obj_sign = args[3].GetBool();

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

void CertificatesHandler::HandleExportCertificate(
    const base::Value::List& args) {
  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 0 /* arg_index */);
  if (!cert_info)
    return;

  net::ScopedCERTCertificateList export_certs;
  export_certs.push_back(net::x509_util::DupCERTCertificate(cert_info->cert()));
  ShowCertExportDialog(web_ui()->GetWebContents(), GetParentWindow(),
                       export_certs.begin(), export_certs.end());
}

void CertificatesHandler::HandleDeleteCertificate(
    const base::Value::List& args) {
  CHECK_EQ(2U, args.size());
  if (!AssignWebUICallbackId(args)) {
    RejectJavascriptCallback(base::Value(args[0].GetString()), base::Value());
    return;
  }

  CertificateManagerModel::CertInfo* cert_info =
      GetCertInfoFromCallbackArgs(args, 1 /* arg_index */);
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

  certificate_manager_model_->RemoveFromDatabase(
      net::x509_util::DupCERTCertificate(cert_info->cert()),
      base::BindOnce(&CertificatesHandler::OnCertificateDeleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CertificatesHandler::OnCertificateDeleted(bool result) {
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
  if (IsJavascriptAllowed()) {
    FireWebUIListener(
        "client-import-allowed-changed",
        base::Value(IsClientCertificateManagementAllowed(Slot::kUser)));
    FireWebUIListener("ca-import-allowed-changed",
                      base::Value(IsCACertificateManagementAllowed(
                          CertificateSource::kImported)));
  }
  certificate_manager_model_->Refresh();
}

void CertificatesHandler::HandleRefreshCertificates(
    const base::Value::List& args) {
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
        base::BindOnce(&CertificatesHandler::OnCertificateManagerModelCreated,
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

  base::Value::List nodes;
  for (auto& org_grouping_map_entry : org_grouping_map) {
    // Populate second level (certs).
    base::Value::List subnodes;
    bool contains_policy_certs = false;
    for (auto& org_cert : org_grouping_map_entry.second) {
      // Move the CertInfo into |cert_info_id_map_|.
      CertificateManagerModel::CertInfo* cert_info = org_cert.get();
      std::string id =
          base::NumberToString(cert_info_id_map_.Add(std::move(org_cert)));

      bool is_extractable = !cert_info->hardware_backed();
#if BUILDFLAG(IS_CHROMEOS)
      is_extractable = false;
#endif

      auto cert_dict =
          base::Value::Dict()
              .Set(kCertificatesHandlerKeyField, id)
              .Set(kCertificatesHandlerNameField, cert_info->name())
              .Set(kCertificatesHandlerCanBeDeletedField,
                   CanDeleteCertificate(cert_info))
              .Set(kCertificatesHandlerCanBeEditedField,
                   CanEditCertificate(cert_info))
              .Set(kCertificatesHandlerUntrustedField, cert_info->untrusted())
              .Set(kCertificatesHandlerPolicyInstalledField,
                   cert_info->source() ==
                       CertificateManagerModel::CertInfo::Source::kPolicy)
              .Set(kCertificatesHandlerWebTrustAnchorField,
                   cert_info->web_trust_anchor())
              // TODO(hshi): This should be determined by testing for PKCS #11
              // CKA_EXTRACTABLE attribute. We may need to use the NSS function
              // PK11_ReadRawAttribute to do that.
              .Set(kCertificatesHandlerExtractableField, is_extractable);
      // TODO(mattm): Other columns.
      subnodes.Append(std::move(cert_dict));

      contains_policy_certs |=
          cert_info->source() ==
          CertificateManagerModel::CertInfo::Source::kPolicy;
    }
    std::sort(subnodes.begin(), subnodes.end(), comparator);

    // Populate first level (org name).
    auto org_dict =
        base::Value::Dict()
            .Set(kCertificatesHandlerKeyField,
                 OrgNameToId(org_grouping_map_entry.first))
            .Set(kCertificatesHandlerNameField, org_grouping_map_entry.first)
            .Set(kCertificatesHandlerContainsPolicyCertsField,
                 contains_policy_certs)
            .Set(kCertificatesHandlerSubnodesField, std::move(subnodes));
    nodes.Append(std::move(org_dict));
  }
  std::sort(nodes.begin(), nodes.end(), comparator);

  if (IsJavascriptAllowed()) {
    FireWebUIListener("certificates-changed", base::Value(tab_name), nodes);
  }
}

void CertificatesHandler::ResolveCallback(const base::ValueView response) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallback(const base::ValueView response) {
  DCHECK(!webui_callback_id_.empty());
  RejectJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallbackWithError(const std::string& title,
                                                  const std::string& error) {
  RejectCallback(base::Value::Dict()
                     .Set(kCertificatesHandlerErrorTitle, title)
                     .Set(kCertificatesHandlerErrorDescription, error));
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

  base::Value::List cert_error_list;
  for (const auto& failure : not_imported) {
    cert_error_list.Append(
        base::Value::Dict()
            .Set(kCertificatesHandlerNameField,
                 x509_certificate_model::GetSubjectDisplayName(
                     failure.certificate.get()))
            .Set(kCertificatesHandlerErrorField,
                 NetErrorToString(failure.net_error)));
  }

  RejectCallback(base::Value::Dict()
                     .Set(kCertificatesHandlerErrorTitle, title)
                     .Set(kCertificatesHandlerErrorDescription, error)
                     .Set(kCertificatesHandlerCertificateErrors,
                          std::move(cert_error_list)));
}

gfx::NativeWindow CertificatesHandler::GetParentWindow() {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

CertificateManagerModel::CertInfo*
CertificatesHandler::GetCertInfoFromCallbackArgs(const base::Value::List& args,
                                                 size_t arg_index) {
  if (arg_index >= args.size())
    return nullptr;
  const auto& arg = args[arg_index];
  if (!arg.is_string())
    return nullptr;

  int32_t cert_info_id = 0;
  if (!base::StringToInt(arg.GetString(), &cert_info_id))
    return nullptr;

  return cert_info_id_map_.Lookup(cert_info_id);
}

bool CertificatesHandler::IsClientCertificateManagementAllowed(Slot slot) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!IsClientCertificateManagementAllowedPolicy(slot)) {
    return false;
  }
#endif  //  BUILDFLAG(IS_CHROMEOS)

  return ShouldDisplayClientCertificates();
}

bool CertificatesHandler::IsCACertificateManagementAllowed(
    CertificateSource source) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/194781831): Currently CA certificates are shared between all
  // profiles for technical reasons. Evaluating the policy independently in each
  // profile would create a policy escape (e.g. if one of profiles is not
  // managed). Therefore make the main profile "own" CA certificates and allow
  // management based on its policy.
  if (!Profile::FromWebUI(web_ui())->IsMainProfile()) {
    return false;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS)
  if (!IsCACertificateManagementAllowedPolicy(source)) {
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return true;
}

bool CertificatesHandler::ShouldDisplayClientCertificates() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/194781831): When secondary profiles in Lacros-Chrome support client
  // certificates, this should be removed and the page should be updated to
  // support them.
  if (!Profile::FromWebUI(web_ui())->IsMainProfile()) {
    return false;
  }
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

  return true;
}

#if BUILDFLAG(IS_CHROMEOS)
bool CertificatesHandler::IsClientCertificateManagementAllowedPolicy(
    Slot slot) {
  Profile* profile = Profile::FromWebUI(web_ui());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile()) {
    // TODO(b/194781831): Currently client certificates are not supported in
    // secondary profiles on Lacros-Chrome. This "return" disables some buttons
    // (e.g. Import, Import&Bind) that wouldn't work anyway. This can be changed
    // when client certificates for secondary profiles are implemented.
    return false;
  }
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)

  PrefService* prefs = profile->GetPrefs();
  auto policy_value = static_cast<ClientCertificateManagementPermission>(
      prefs->GetInteger(prefs::kClientCertificateManagementAllowed));

  if (slot == Slot::kUser) {
    return policy_value != ClientCertificateManagementPermission::kNone;
  }
  return policy_value == ClientCertificateManagementPermission::kAll;
}

bool CertificatesHandler::IsCACertificateManagementAllowedPolicy(
    CertificateSource source) {
  Profile* profile = Profile::FromWebUI(web_ui());

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile->IsMainProfile()) {
    // TODO(b/194781831): Currently CA certificates are shared between all
    // profiles for technical reasons. Therefore only the main profile should
    // decide if they are allowed to be managed. This can be changed when a
    // proper separation of CA certificates between profiles is implemented.
    return false;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

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
#endif  // BUILDFLAG(IS_CHROMEOS)

bool CertificatesHandler::CanDeleteCertificate(
    const CertificateManagerModel::CertInfo* cert_info) {
  if (!cert_info->can_be_deleted() ||
      cert_info->source() ==
          CertificateManagerModel::CertInfo::Source::kPolicy) {
    return false;
  }

  if (cert_info->type() == net::CertType::USER_CERT) {
    return IsClientCertificateManagementAllowed(
        cert_info->device_wide() ? Slot::kSystem : Slot::kUser);
  }
  if (cert_info->type() == net::CertType::CA_CERT) {
    CertificateSource source = cert_info->can_be_deleted()
                                   ? CertificateSource::kImported
                                   : CertificateSource::kBuiltIn;
    return IsCACertificateManagementAllowed(source);
  }
  return true;
}

bool CertificatesHandler::CanEditCertificate(
    const CertificateManagerModel::CertInfo* cert_info) {
  if ((cert_info->type() != net::CertType::CA_CERT) ||
      (cert_info->source() ==
       CertificateManagerModel::CertInfo::Source::kPolicy)) {
    return false;
  }

  CertificateSource source = cert_info->can_be_deleted()
                                 ? CertificateSource::kImported
                                 : CertificateSource::kBuiltIn;
  return IsCACertificateManagementAllowed(source);
}

#if BUILDFLAG(IS_CHROMEOS)
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
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace certificate_manager
