// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"

#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/ec_private_key.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "crypto/ec_private_key.h"

namespace client_certificates {

namespace {

scoped_refptr<ECPrivateKey> CreateKey() {
  auto key = crypto::ECPrivateKey::Create();
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<ECPrivateKey>(std::move(key));
}

}  // namespace

ECPrivateKeyFactory::ECPrivateKeyFactory() = default;

ECPrivateKeyFactory::~ECPrivateKeyFactory() = default;

void ECPrivateKeyFactory::CreatePrivateKey(PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(CreateKey),
                                               std::move(callback));
}

}  // namespace client_certificates
