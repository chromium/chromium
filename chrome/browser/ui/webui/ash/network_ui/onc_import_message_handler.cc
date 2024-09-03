// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/network_ui/onc_import_message_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/onc/network_onc_utils.h"
#include "chromeos/ash/components/network/onc/onc_certificate_importer_impl.h"
#include "chromeos/components/onc/onc_parsed_certificates.h"
#include "chromeos/components/onc/onc_utils.h"
#include "components/onc/onc_constants.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

void GetCertDBOnIOThread(
    NssCertDatabaseGetter database_getter,
    base::OnceCallback<void(net::NSSCertDatabase*)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  net::NSSCertDatabase* cert_db =
      std::move(database_getter).Run(std::move(split_callback.first));
  if (cert_db)
    std::move(split_callback.second).Run(cert_db);
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
  std::string callback_id = list[0].GetString();
  std::string onc_blob = list[1].GetString();
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
          onc_blob, onc_source, /*passphrase=*/std::string(), &network_configs,
          &global_network_config, &certificates)) {
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
    if (!num_networks_imported)
      has_error = true;
    Respond(callback_id, result, has_error);
    return;
  }

  auto cert_importer = std::make_unique<onc::CertificateImporterImpl>(
      content::GetIOThreadTaskRunner({}), nssdb);
  auto certs =
      std::make_unique<chromeos::onc::OncParsedCertificates>(certificates);
  if (certs->has_error()) {
    has_error = true;
    result += "Some certificates could not be parsed.\n";
  }
  auto* const cert_importer_ptr = cert_importer.get();
  cert_importer_ptr->ImportAllCertificatesUserInitiated(
      certs->server_or_authority_certificates(), certs->client_certificates(),
      base::BindOnce(&OncImportMessageHandler::OnCertificatesImported,
                     weak_factory_.GetWeakPtr(), std::move(cert_importer),
                     callback_id, result, has_error));
}

void OncImportMessageHandler::OnCertificatesImported(
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

}  // namespace ash
