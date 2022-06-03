// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/filesystem_proxy_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"

namespace storage {

namespace {

std::unique_ptr<FilesystemProxy> CreateUnrestrictedFilesystemProxy() {
  return std::make_unique<FilesystemProxy>(FilesystemProxy::UNRESTRICTED,
                                           base::FilePath());
}

FilesystemProxyFactory& GetFactory() {
  static base::NoDestructor<FilesystemProxyFactory> factory{
      base::BindRepeating(&CreateUnrestrictedFilesystemProxy)};
  return *factory;
}

}  // namespace

void SetFilesystemProxyFactory(FilesystemProxyFactory factory) {
  GetFactory() = std::move(factory);
}

std::unique_ptr<FilesystemProxy> CreateFilesystemProxy() {
  return GetFactory().Run();
}

}  // namespace storage
