// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/unguessable_token.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/invitation.h"

namespace drivefs {

// Awaits for connection from DriveFS.
class COMPONENT_EXPORT(DRIVEFS) DriveFsBootstrapListener {
 public:
  DriveFsBootstrapListener();
  virtual ~DriveFsBootstrapListener();

  const base::UnguessableToken& pending_token() const { return pending_token_; }
  virtual mojo::PendingRemote<mojom::DriveFsBootstrap> bootstrap();
  bool is_connected() const { return connected_; }

 protected:
  // Protected for stubbing out for testing.
  virtual void SendInvitationOverPipe(base::ScopedFD handle);

 private:
  void AcceptMojoConnection(base::ScopedFD handle);

  mojo::OutgoingInvitation invitation_;
  mojo::PendingRemote<mojom::DriveFsBootstrap> bootstrap_;

  // The token passed to DriveFS as part of 'source path' used to match it to
  // this instance.
  base::UnguessableToken pending_token_;

  bool connected_ = false;

  DISALLOW_COPY_AND_ASSIGN(DriveFsBootstrapListener);
};

// Establishes and holds mojo connection to DriveFS.
class COMPONENT_EXPORT(DRIVEFS) DriveFsConnection {
 public:
  DriveFsConnection() = default;
  virtual ~DriveFsConnection() = default;
  virtual base::UnguessableToken Connect(mojom::DriveFsDelegate* delegate,
                                         base::OnceClosure on_disconnected) = 0;
  virtual mojom::DriveFs& GetDriveFs() = 0;

  static std::unique_ptr<DriveFsConnection> Create(
      std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener,
      mojom::DriveFsConfigurationPtr config);

 private:
  DISALLOW_COPY_AND_ASSIGN(DriveFsConnection);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_
