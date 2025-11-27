// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_TEACHER_SCREEN_PRESENTER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_TEACHER_SCREEN_PRESENTER_H_

#include <string_view>

#include "base/functional/callback_forward.h"

namespace boca {
class UserIdentity;
}  // namespace boca

namespace ash::boca {

class TeacherScreenPresenter {
 public:
  TeacherScreenPresenter(const TeacherScreenPresenter&) = delete;
  TeacherScreenPresenter& operator=(const TeacherScreenPresenter&) = delete;

  virtual ~TeacherScreenPresenter() = default;

  virtual void Start(std::string_view receiver_id,
                     std::string_view receiver_name,
                     ::boca::UserIdentity teacher_identity,
                     const bool is_session_active,
                     base::OnceCallback<void(bool)> success_cb,
                     base::OnceClosure disconnected_cb) = 0;

  virtual void Stop(base::OnceCallback<void(bool)> success_cb) = 0;

  virtual bool IsPresenting() = 0;

 protected:
  TeacherScreenPresenter() = default;
};

}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_TEACHER_SCREEN_PRESENTER_H_
