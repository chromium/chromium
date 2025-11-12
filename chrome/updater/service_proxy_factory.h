// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_
#define CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace updater {

enum class UpdaterScope;
class UpdateService;
class UpdateServiceInternal;

scoped_refptr<UpdateService> CreateUpdateServiceProxy(
    UpdaterScope updater_scope,
    base::TimeDelta get_version_timeout = base::TimeDelta::Max());

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxy(
    UpdaterScope updater_scope);

#if BUILDFLAG(IS_WIN)
scoped_refptr<UpdateService> CreateUpdateServiceProxyMojo(
    UpdaterScope updater_scope,
    base::TimeDelta get_version_timeout = base::TimeDelta::Max());

scoped_refptr<UpdateServiceInternal> CreateUpdateServiceInternalProxyMojo(
    UpdaterScope updater_scope);
#endif  // BUILDFLAG(IS_WIN)

}  // namespace updater

#endif  // CHROME_UPDATER_SERVICE_PROXY_FACTORY_H_
