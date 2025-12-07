// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_STUDENT_SCREEN_PRESENTER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_STUDENT_SCREEN_PRESENTER_H_

#include <optional>
#include <string_view>

#include "base/functional/callback_forward.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace ash::boca {

class StudentScreenPresenter {
 public:
  StudentScreenPresenter(const StudentScreenPresenter&) = delete;
  StudentScreenPresenter& operator=(const StudentScreenPresenter&) = delete;

  virtual ~StudentScreenPresenter() = default;

  virtual void Start(std::string_view receiver_id,
                     const ::boca::UserIdentity& student_identity,
                     std::string_view student_device_id,
                     base::OnceCallback<void(bool)> success_cb,
                     base::OnceClosure disconnected_cb) = 0;

  virtual void CheckConnection() = 0;

  virtual void Stop(base::OnceCallback<void(bool)> success_cb) = 0;

  virtual bool IsPresenting(std::optional<std::string_view> student_id) = 0;

 protected:
  StudentScreenPresenter() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_STUDENT_SCREEN_PRESENTER_H_
