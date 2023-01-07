// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_

#include <memory>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/invitation.h"

namespace drivefs {

// Awaits for connection from DriveFS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
    DriveFsBootstrapListener {
 public:
  DriveFsBootstrapListener();

  DriveFsBootstrapListener(const DriveFsBootstrapListener&) = delete;
  DriveFsBootstrapListener& operator=(const DriveFsBootstrapListener&) = delete;

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
};

// Establishes and holds mojo connection to DriveFS.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsConnection {
 public:
  DriveFsConnection() = default;

  DriveFsConnection(const DriveFsConnection&) = delete;
  DriveFsConnection& operator=(const DriveFsConnection&) = delete;

  virtual ~DriveFsConnection() = default;
  virtual base::UnguessableToken Connect(mojom::DriveFsDelegate* delegate,
                                         base::OnceClosure on_disconnected) = 0;
  virtual mojom::DriveFs& GetDriveFs() = 0;

  static std::unique_ptr<DriveFsConnection> Create(
      std::unique_ptr<DriveFsBootstrapListener> bootstrap_listener,
      mojom::DriveFsConfigurationPtr config);
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_BOOTSTRAP_H_
