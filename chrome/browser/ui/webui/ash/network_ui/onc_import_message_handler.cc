// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/network_ui/onc_import_message_handler.h"

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/net/server_certificate_database_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/server_certificate_database/server_certificate_database.h"
#include "components/server_certificate_database/server_certificate_database.pb.h"
#include "components/server_certificate_database/server_certificate_database_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
namespace ash {

namespace {

void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (cert_db) {
    std::move(split_callback.second).Run(cert_db);
  }
}

}  // namespace

OncImportMessageHandler::OncImportMessageHandler() = default;

OncImportMessageHandler::~OncImportMessageHandler() = default;

void OncImportMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "importONC", base::BindRepeating(&OncImportMessageHandler::OnImportONC,
                                       base::Unretained(this)));
}

void OncImportMessageHandler::Respond(const std::string& callback_id,
                                      const std::string& result,
                                      bool is_error) {
  base::Value::List response;
  response.Append(result);
  response.Append(is_error);
  ResolveJavascriptCallback(base::Value(callback_id), response);
}

void OncImportMessageHandler::OnImportONC(const base::Value::List& list) {
  CHECK_EQ(2u, list.size());
  const std::string& callback_id = list[0].GetString();
  const std::string& onc_blob = list[1].GetString();
  AllowJavascript();

  // TODO(crbug.com/40753707): Pass the `NssCertDatabaseGetter` to
  // the `CertImporter`. This is not unsafe if the profile shuts down during
  // this operation.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetCertDBOnIOThread,
          NssServiceFactory::GetForContext(Profile::FromWebUI(web_ui()))
              ->CreateNSSCertDatabaseGetterForIOThread(),
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &OncImportMessageHandler::ImportONCToNSSDB,
              weak_factory_.GetWeakPtr(), callback_id, onc_blob))));
}

void OncImportMessageHandler::ImportONCToNSSDB(const std::string& callback_id,
                                               const std::string& onc_blob,
                                               net::NSSCertDatabase* nssdb) {
  const user_manager::User* user =
      ProfileHelper::Get()->GetUserByProfile(Profile::FromWebUI(web_ui()));
  if (!user) {
    Respond(callback_id, "User not found.", /*is_error=*/true);
    return;
  }

  std::string result;
  bool has_error = false;

  ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_USER_IMPORT;
  base::Value::List network_configs;
  base::Value::Dict global_network_config;
  base::Value::List certificates;
  if (!chromeos::onc::ParseAndValidateOncForImport(
          onc_blob, onc_source, &network_configs, &global_network_config,
          &certificates)) {
    has_error = true;
    result += "Errors occurred during ONC parsing.\n";
  }

  std::string import_error;
  int num_networks_imported =
      onc::ImportNetworksForUser(user, network_configs, &import_error);
  if (!import_error.empty()) {
    has_error = true;
    result += "Error importing networks: " + import_error + "\n";
  }
  result +=
      base::StringPrintf("Networks imported: %d\n", num_networks_imported);
  if (certificates.empty()) {
    if (!num_networks_imported) {
      has_error = true;
    }
    Respond(callback_id, result, has_error);
    return;
  }

  std::unique_ptr<chromeos::onc::OncParsedCertificates> certs =
      std::make_unique<chromeos::onc::OncParsedCertificates>(certificates);
  if (certs->has_error()) {
    has_error = true;
    result += "Some certificates could not be parsed.\n";
  }

  if (certs->server_or_authority_certificates().empty() &&
      certs->client_certificates().empty()) {
    Respond(callback_id, result, has_error);
    return;
  }

  auto cert_importer = std::make_unique<onc::CertificateImporterImpl>(
      content::GetIOThreadTaskRunner({}), nssdb);
  onc::CertificateImporterImpl* cert_importer_ptr = cert_importer.get();

  const std::vector<chromeos::onc::OncParsedCertificates::ClientCertificate>&
      client_certs = certs->client_certificates();
  // Import client certs first, and then do server certificates separately.
  cert_importer_ptr->ImportClientCertificates(
      client_certs,
      base::BindOnce(&OncImportMessageHandler::GetAllServerCertificates,
                     weak_factory_.GetWeakPtr(), std::move(cert_importer),
                     std::move(certs), callback_id, result, has_error));
}

void OncImportMessageHandler::GetAllServerCertificates(
    std::unique_ptr<onc::CertificateImporterImpl> cert_importer,
    std::unique_ptr<chromeos::onc::OncParsedCertificates> certs,
    const std::string& callback_id,
    const std::string& previous_result,
    bool has_error,
    bool cert_import_success) {
  std::string result = previous_result;
  if (!cert_import_success) {
    has_error = true;
    result += "Some client certificates couldn't be imported.\n";
  }

  if (certs->server_or_authority_certificates().empty()) {
    Respond(callback_id, result, has_error);
    return;
  }

  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (!server_cert_service) {
    has_error = true;
    result += "Server certificates could not be imported.\n";
    Respond(callback_id, result, has_error);
    return;
  }
  // Fetch the current server certs in the DB.
  server_cert_service->GetAllCertificates(
      base::BindOnce(&OncImportMessageHandler::ImportServerCertificates,
                     weak_factory_.GetWeakPtr(), std::move(certs), callback_id,
                     result, has_error));
  // |cert_importer| will be destroyed when the callback exits.
}

void OncImportMessageHandler::ImportServerCertificates(
    std::unique_ptr<chromeos::onc::OncParsedCertificates> certs,
    const std::string& callback_id,
    const std::string& previous_result,
    bool has_error,
    std::vector<net::ServerCertificateDatabase::CertInformation>
        current_certs) {
  std::string result = previous_result;
  net::ServerCertificateDatabaseService* server_cert_service =
      net::ServerCertificateDatabaseServiceFactory::GetForBrowserContext(
          Profile::FromWebUI(web_ui()));
  if (!server_cert_service) {
    has_error = true;
    result += "Server certificates could not be imported.\n";
    Respond(callback_id, result, has_error);
    return;
  }
  std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
  for (const auto& cert : certs->server_or_authority_certificates()) {
    scoped_refptr<net::X509Certificate> cert_to_import = cert.certificate();

    net::ServerCertificateDatabase::CertInformation cert_info(
        cert_to_import->cert_span());

    bool found = false;
    for (const auto& current_cert : current_certs) {
      found |= current_cert.sha256hash_hex == cert_info.sha256hash_hex;
    }

    // Don't change a cert's metadata if its already in the cert store.
    if (found) {
      result += "Server certificate with hash " + cert_info.sha256hash_hex +
                " already in cert store.\n";
    } else {
      cert_info.cert_metadata.mutable_trust()->set_trust_type(
          cert.web_trust_requested()
              ? chrome_browser_server_certificate_database::CertificateTrust::
                    CERTIFICATE_TRUST_TYPE_TRUSTED
              : chrome_browser_server_certificate_database::CertificateTrust::
                    CERTIFICATE_TRUST_TYPE_UNSPECIFIED);

      cert_infos.push_back(std::move(cert_info));
    }
  }

  if (cert_infos.empty()) {
    Respond(callback_id, result, has_error);
    return;
  }

  server_cert_service->AddOrUpdateUserCertificates(
      std::move(cert_infos),
      base::BindOnce(&OncImportMessageHandler::OnServerCertsImportedDb,
                     weak_factory_.GetWeakPtr(), callback_id, result,
                     has_error));
  // |certs| will be destroyed when the callback exits.
}

void OncImportMessageHandler::OnAllCertificatesImportedUserInitiated(
    std::unique_ptr<onc::CertificateImporterImpl> cert_importer,
    const std::string& callback_id,
    const std::string& previous_result,
    bool has_error,
    bool cert_import_success) {
  std::string result = previous_result;
  if (!cert_import_success) {
    has_error = true;
    result += "Some certificates couldn't be imported.\n";
  }
  Respond(callback_id, result, has_error);
  // |cert_importer| will be destroyed when the callback exits.
}

void OncImportMessageHandler::OnServerCertsImportedDb(
    const std::string& callback_id,
    const std::string& previous_result,
    bool has_error,
    bool cert_import_success) {
  std::string result = previous_result;
  if (!cert_import_success) {
    has_error = true;
    result += "Server certificates couldn't be imported.\n";
  }
  Respond(callback_id, result, has_error);
}

}  // namespace ash
