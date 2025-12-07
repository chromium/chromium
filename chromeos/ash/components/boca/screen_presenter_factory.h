// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SCREEN_PRESENTER_FACTORY_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SCREEN_PRESENTER_FACTORY_H_

#include <memory>
#include <string_view>

namespace boca {
class UserIdentity;
}  // namespace boca

namespace ash::boca {

class StudentScreenPresenter;
class TeacherScreenPresenter;

class ScreenPresenterFactory {
 public:
  ScreenPresenterFactory(const ScreenPresenterFactory&) = delete;
  ScreenPresenterFactory& operator=(const ScreenPresenterFactory&) = delete;

  virtual ~ScreenPresenterFactory() = default;

  virtual std::unique_ptr<StudentScreenPresenter> CreateStudentScreenPresenter(
      std::string_view session_id,
      const ::boca::UserIdentity& teacher_identity,
      std::string_view teacher_device_id) = 0;

  virtual std::unique_ptr<TeacherScreenPresenter> CreateTeacherScreenPresenter(
      std::string_view teacher_device_id) = 0;

 protected:
  ScreenPresenterFactory() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SCREEN_PRESENTER_FACTORY_H_
