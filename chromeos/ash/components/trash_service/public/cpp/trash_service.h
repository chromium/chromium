// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_SERVICE_H_
#define CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_SERVICE_H_

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::trash_service {

using ParseTrashInfoCallback = base::OnceCallback<
    void(base::File::Error, const base::FilePath&, base::Time)>;

// Starts up an out-of-process Trash service, binds receiver and returns the
// pending remote.
// TODO(b/238943248): This should not launch an individual service every time
// but should re-use one if it's already running.
mojo::PendingRemote<mojom::TrashService> LaunchTrashService();

// Overrides the logic used by |LaunchTrashService()| to produce a remote
// service, allowing tests to set up an in-process instance to be used instead
// of an out-of-process instance.
using LaunchCallback =
    base::RepeatingCallback<mojo::PendingRemote<mojom::TrashService>()>;
void SetTrashServiceLaunchOverrideForTesting(LaunchCallback callback);

}  // namespace ash::trash_service

#endif  // CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_PUBLIC_CPP_TRASH_SERVICE_H_
