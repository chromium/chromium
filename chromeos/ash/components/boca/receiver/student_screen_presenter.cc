// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/student_screen_presenter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/components/boca/boca_request.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/get_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/start_kiosk_receiver_request.h"
#include "chromeos/ash/components/boca/receiver/update_kiosk_receiver_state_request.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {
namespace {

std::unique_ptr<google_apis::RequestSender> CreateSender(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  std::vector<std::string> scopes = {kSchoolToolsAuthScope};
  auto auth_service = std::make_unique<google_apis::AuthService>(
      identity_manager,
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
      url_loader_factory, scopes);
  return std::make_unique<google_apis::RequestSender>(
      std::move(auth_service), url_loader_factory,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           /* `USER_VISIBLE` is because the requested/returned data is visible
               to the user on Web UI surfaces. */
           base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}),
      /*custom_user_agent=*/"", traffic_annotation);
}

}  // namespace

StudentScreenPresenterImpl::StudentScreenPresenterImpl(
    const ::boca::UserIdentity& teacher_identity,
    std::string_view teacher_device_id,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : teacher_identity_(teacher_identity),
      teacher_device_id_(teacher_device_id),
      url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

StudentScreenPresenterImpl::~StudentScreenPresenterImpl() = default;

void StudentScreenPresenterImpl::Start(
    std::string_view session_id,
    std::string_view receiver_id,
    const ::boca::UserIdentity& student_identity,
    std::string_view student_device_id,
    base::OnceCallback<void(bool)> success_cb,
    base::OnceClosure disconnected_cb) {
  if (session_id_.has_value()) {
    LOG(ERROR) << "[Boca] Trying to present more than one student screen";
    std::move(success_cb).Run(false);
    return;
  }
  session_id_ = session_id;
  receiver_id_ = receiver_id;
  disconnected_cb_ = std::move(disconnected_cb);
  start_connection_request_sender_ = CreateSender(
      url_loader_factory_, identity_manager_,
      boca_receiver::StartKioskReceiverRequest::kTrafficAnnotation);
  auto start_request_delegate =
      std::make_unique<boca_receiver::StartKioskReceiverRequest>(
          receiver_id_.value(), teacher_identity_, student_identity,
          teacher_device_id_, std::string(student_device_id),
          /*connection_code=*/std::nullopt, session_id_,
          base::BindOnce(&StudentScreenPresenterImpl::OnStartResponse,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(success_cb)));
  auto start_request =
      std::make_unique<BocaRequest>(start_connection_request_sender_.get(),
                                    std::move(start_request_delegate));
  start_connection_request_sender_->StartRequestWithAuthRetry(
      std::move(start_request));
}

void StudentScreenPresenterImpl::OnStartResponse(
    base::OnceCallback<void(bool)> success_cb,
    std::optional<std::string> connection_id) {
  if (!connection_id.has_value()) {
    std::move(success_cb).Run(false);
    Reset();
    return;
  }
  connection_id_ = connection_id;
  std::move(success_cb).Run(true);
}

void StudentScreenPresenterImpl::Reset() {
  receiver_id_.reset();
  session_id_.reset();
  connection_id_.reset();
  disconnected_cb_.Reset();
}

}  // namespace ash::boca
