// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_FILESYSTEM_PROXY_FACTORY_H_
#define COMPONENTS_SERVICES_STORAGE_FILESYSTEM_PROXY_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "components/services/storage/public/cpp/filesystem/filesystem_proxy.h"

namespace storage {

// Replaces the default FilesystemProxy factory with a custom one. |factory|
// must be safe to call from any thread.
using FilesystemProxyFactory =
    base::RepeatingCallback<std::unique_ptr<FilesystemProxy>()>;
COMPONENT_EXPORT(FILESYSTEM_PROXY_FACTORY)
void SetFilesystemProxyFactory(FilesystemProxyFactory factory);

// Produces a new FilesystemProxy suitable for use in the service's current
// execution environment. If the service is unsandboxed (or running e.g. in a
// browser process) this will produce an unrestricted FilesystemProxy which can
// access any path. If the service is sandboxed, this will produce a restricted
// FilesystemProxy which can only traverse a limited scope of filesystem, and
// only through IPC to a more privileged process.
COMPONENT_EXPORT(FILESYSTEM_PROXY_FACTORY)
std::unique_ptr<FilesystemProxy> CreateFilesystemProxy();

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_FILESYSTEM_PROXY_FACTORY_H_
