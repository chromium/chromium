// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_PROVIDER_H_
#define COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_PROVIDER_H_

#include "base/component_export.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"

namespace leveldb_proto {

class SharedProtoDatabase;
class ProtoDatabaseProvider;

// Helper class to be instantiated for each request for a shared database
// provider to be used in the wrapper. |client_task_runner| is the
// SequencedTaskRunner provided by the main provider so its WeakPtrs are
// always checked on the right sequence.
class COMPONENT_EXPORT(LEVELDB_PROTO) SharedProtoDatabaseProvider {
 public:
  using GetSharedDBInstanceCallback =
      base::OnceCallback<void(scoped_refptr<SharedProtoDatabase>)>;

  ~SharedProtoDatabaseProvider();

  void GetDBInstance(
      GetSharedDBInstanceCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);

 private:
  friend class ProtoDatabaseProvider;
  friend class TestSharedProtoDatabaseProvider;

  SharedProtoDatabaseProvider(
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr);

  scoped_refptr<base::SequencedTaskRunner> client_task_runner_;
  base::WeakPtr<ProtoDatabaseProvider> provider_weak_ptr_;
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_INTERNAL_SHARED_PROTO_DATABASE_PROVIDER_H_