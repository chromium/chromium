// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
#define CHROMEOS_COMPONENTS_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_

namespace signin {
class IdentityManager;
}  // namespace signin

namespace chromeos {

// Defines interface to access Browser side functionalities for the
// ProjectorApp.
class ProjectorAppClient {
 public:
  ProjectorAppClient(const ProjectorAppClient&) = delete;
  ProjectorAppClient& operator=(const ProjectorAppClient&) = delete;

  static ProjectorAppClient* Get();

  // Returns the IdentityManager for the primary user profile.
  virtual signin::IdentityManager* GetIdentityManager() = 0;

 protected:
  ProjectorAppClient();
  virtual ~ProjectorAppClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_PROJECTOR_APP_PROJECTOR_APP_CLIENT_H_
