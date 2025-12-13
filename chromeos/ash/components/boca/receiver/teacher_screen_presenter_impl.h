// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_TEACHER_SCREEN_PRESENTER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_TEACHER_SCREEN_PRESENTER_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/notifications/boca_notification_handler.h"
#include "chromeos/ash/components/boca/proto/receiver.pb.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/teacher_screen_presenter.h"

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

class SharedCrdSessionWrapper;

class TeacherScreenPresenterImpl : public TeacherScreenPresenter {
 public:
  TeacherScreenPresenterImpl(
      std::string_view teacher_device_id,
      std::unique_ptr<SharedCrdSessionWrapper> shared_crd_session,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  TeacherScreenPresenterImpl(const TeacherScreenPresenterImpl&) = delete;
  TeacherScreenPresenterImpl& operator=(const TeacherScreenPresenterImpl&) =
      delete;

  ~TeacherScreenPresenterImpl() override;

  // TeacherScreenPresenter:
  void Start(std::string_view receiver_id,
             std::string_view receiver_name,
             ::boca::UserIdentity teacher_identity,
             const bool is_session_active,
             base::OnceCallback<void(bool)> success_cb,
             base::OnceClosure disconnected_cb) override;
  void Stop(base::OnceCallback<void(bool)> success_cb) override;
  bool IsPresenting() override;

 private:
  void OnGetReceiverResponse(::boca::UserIdentity teacher_identity,
                             std::optional<::boca::KioskReceiver> receiver);

  void OnGetCrdConnectionCode(::boca::UserIdentity teacher_identity,
                              const std::string& connection_code);

  void OnStartReceiverResponse(std::optional<std::string> connection_id);

  void OnCrdSessionFinished();

  void Reset();

  const std::string teacher_device_id_;
  BocaNotificationHandler notification_handler_;
  const std::unique_ptr<SharedCrdSessionWrapper> shared_crd_session_;
  const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  std::optional<std::string> receiver_id_;
  std::optional<std::string> receiver_name_;
  bool is_session_active_;
  base::OnceCallback<void(bool)> start_success_cb_;
  base::OnceClosure disconnected_cb_;
  std::unique_ptr<google_apis::RequestSender> get_receiver_request_sender_;
  std::unique_ptr<google_apis::RequestSender> start_connection_request_sender_;

  base::WeakPtrFactory<TeacherScreenPresenterImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_TEACHER_SCREEN_PRESENTER_IMPL_H_
