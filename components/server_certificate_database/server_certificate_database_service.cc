// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_service.h"

#include <string>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/server_certificate_database/server_certificate_database.h"

namespace net {

ServerCertificateDatabaseService::ServerCertificateDatabaseService(
    base::FilePath profile_path)
    : profile_path_(std::move(profile_path)) {
  server_cert_database_ = base::SequenceBound<net::ServerCertificateDatabase>(
      base::ThreadPool::CreateSequencedTaskRunnerForResource(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN},
          profile_path_.Append(kServerCertificateDatabaseName)),
      profile_path_);
}

ServerCertificateDatabaseService::~ServerCertificateDatabaseService() = default;

void ServerCertificateDatabaseService::AddOrUpdateUserCertificates(
    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::InsertOrUpdateCerts)
      .WithArgs(std::move(cert_infos))
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ServerCertificateDatabaseService::GetAllCertificates(
    base::OnceCallback<
        void(std::vector<net::ServerCertificateDatabase::CertInformation>)>
        callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveAllCertificates)
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::GetCertificatesCount(
    base::OnceCallback<void(uint32_t)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::RetrieveCertificatesCount)
      .Then(std::move(callback));
}

void ServerCertificateDatabaseService::DeleteCertificate(
    const std::string& sha256hash_hex,
    base::OnceCallback<void(bool)> callback) {
  server_cert_database_
      .AsyncCall(&net::ServerCertificateDatabase::DeleteCertificate)
      .WithArgs(sha256hash_hex)
      .Then(base::BindOnce(
          &ServerCertificateDatabaseService::HandleModificationResult,
          weak_factory_.GetWeakPtr(), std::move(callback)));
}

base::CallbackListSubscription ServerCertificateDatabaseService::AddObserver(
    base::RepeatingClosure callback) {
  return observers_.Add(std::move(callback));
}

void ServerCertificateDatabaseService::HandleModificationResult(
    base::OnceCallback<void(bool)> callback,
    bool success) {
  std::move(callback).Run(success);
  if (success) {
    observers_.Notify();
  }
}

}  // namespace net
