// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/gemini/gemini_status_fetcher.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/gemini/get_gemini_status_request.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {
namespace {

const char kGeminiEnabledPref[] = "ash.boca.gemini_enabled";

std::unique_ptr<google_apis::RequestSender> CreateSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory,
      signin::OAuthConsumerId::kChromeOsBocaSchoolToolsAuth);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/"", traffic_annotation);
}

}  // namespace

GeminiStatusFetcher::GeminiStatusFetcher(
    std::string gaia_id,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service)
    : gaia_id_(std::move(gaia_id)),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service) {}

GeminiStatusFetcher::~GeminiStatusFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Cancel any pending callbacks if destroyed before requests complete.
  bool current_status = pref_service_->GetBoolean(kGeminiEnabledPref);
  while (!callbacks_.empty()) {
    std::move(callbacks_.front()).Run(current_status);
    callbacks_.pop();
  }
}

// static
void GeminiStatusFetcher::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  CHECK(registry);
  registry->RegisterBooleanPref(kGeminiEnabledPref, true);
}

void GeminiStatusFetcher::GetStatus(GetStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.push(std::move(callback));
  if (request_in_progress_) {
    return;
  }
  GetStatusInternal();
}

void GeminiStatusFetcher::GetStatusInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request_in_progress_ = true;
  request_sender_ = CreateSender(url_loader_factory_, identity_manager_,
                                 GetGeminiStatusRequest::kTrafficAnnotation);
  auto request_delegate = std::make_unique<GetGeminiStatusRequest>(
      gaia_id_, base::BindOnce(&GeminiStatusFetcher::OnStatusResponse,
                               weak_ptr_factory_.GetWeakPtr()));
  auto request = std::make_unique<BocaRequest>(request_sender_.get(),
                                               std::move(request_delegate));
  request_sender_->StartRequestWithAuthRetry(std::move(request));
}

void GeminiStatusFetcher::OnStatusResponse(std::optional<bool> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request_in_progress_ = false;
  bool final_status = pref_service_->GetBoolean(kGeminiEnabledPref);
  if (result.has_value()) {
    final_status = result.value();
    pref_service_->SetBoolean(kGeminiEnabledPref, final_status);
  }

  while (!callbacks_.empty()) {
    std::move(callbacks_.front()).Run(final_status);
    callbacks_.pop();
  }
}

}  // namespace ash::boca
