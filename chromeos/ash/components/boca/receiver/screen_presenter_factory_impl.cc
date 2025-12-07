// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/receiver/screen_presenter_factory_impl.h"

#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/receiver/student_screen_presenter_impl.h"
#include "chromeos/ash/components/boca/receiver/teacher_screen_presenter_impl.h"
#include "chromeos/ash/components/boca/shared_crd_session_wrapper.h"
#include "chromeos/ash/components/boca/student_screen_presenter.h"
#include "chromeos/ash/components/boca/teacher_screen_presenter.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash::boca {

ScreenPresenterFactoryImpl::ScreenPresenterFactoryImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager) {}

ScreenPresenterFactoryImpl::~ScreenPresenterFactoryImpl() = default;

std::unique_ptr<StudentScreenPresenter>
ScreenPresenterFactoryImpl::CreateStudentScreenPresenter(
    std::string_view session_id,
    const ::boca::UserIdentity& teacher_identity,
    std::string_view teacher_device_id) {
  return std::make_unique<StudentScreenPresenterImpl>(
      session_id, teacher_identity, teacher_device_id, url_loader_factory_,
      identity_manager_);
}

std::unique_ptr<TeacherScreenPresenter>
ScreenPresenterFactoryImpl::CreateTeacherScreenPresenter(
    std::string_view teacher_device_id) {
  return std::make_unique<TeacherScreenPresenterImpl>(
      teacher_device_id, BocaAppClient::Get()->CreateSharedCrdSessionWrapper(),
      url_loader_factory_, identity_manager_);
}

}  // namespace ash::boca
