// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BLOB_CONTEXT_GETTER_FACTORY_H_
#define COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BLOB_CONTEXT_GETTER_FACTORY_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace storage {
class BlobStorageContext;
}  // namespace storage

namespace download {

using BlobContextGetter =
    base::RepeatingCallback<base::WeakPtr<storage::BlobStorageContext>()>;
using BlobContextGetterCallback = base::OnceCallback<void(BlobContextGetter)>;

// Retrieves a blob storage context getter on main thread.
class BlobContextGetterFactory {
 public:
  virtual void RetrieveBlobContextGetter(
      BlobContextGetterCallback callback) = 0;

  BlobContextGetterFactory(const BlobContextGetterFactory&) = delete;
  BlobContextGetterFactory& operator=(const BlobContextGetterFactory&) = delete;

  virtual ~BlobContextGetterFactory() = default;

 protected:
  BlobContextGetterFactory() = default;
};

using BlobContextGetterFactoryPtr = std::unique_ptr<BlobContextGetterFactory>;

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_PUBLIC_BACKGROUND_SERVICE_BLOB_CONTEXT_GETTER_FACTORY_H_
