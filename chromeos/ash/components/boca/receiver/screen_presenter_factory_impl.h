// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_SCREEN_PRESENTER_FACTORY_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_SCREEN_PRESENTER_FACTORY_IMPL_H_

#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chromeos/ash/components/boca/screen_presenter_factory.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class IdentityManager;
}  // namespace signin

namespace ash::boca {

class StudentScreenPresenter;
class TeacherScreenPresenter;

class ScreenPresenterFactoryImpl : public ScreenPresenterFactory {
 public:
  ScreenPresenterFactoryImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  ScreenPresenterFactoryImpl(const ScreenPresenterFactoryImpl&) = delete;
  ScreenPresenterFactoryImpl& operator=(const ScreenPresenterFactoryImpl&) =
      delete;

  ~ScreenPresenterFactoryImpl() override;

  // ScreenPresenterFactory:
  std::unique_ptr<StudentScreenPresenter> CreateStudentScreenPresenter(
      std::string_view session_id,
      const ::boca::UserIdentity& teacher_identity,
      std::string_view teacher_device_id) override;
  std::unique_ptr<TeacherScreenPresenter> CreateTeacherScreenPresenter(
      std::string_view teacher_device_id) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  raw_ptr<signin::IdentityManager> identity_manager_;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_SCREEN_PRESENTER_FACTORY_IMPL_H_
