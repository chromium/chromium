// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/transactional_leveldb/leveldb_write_batch.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_piece.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace content {

std::unique_ptr<LevelDBWriteBatch> LevelDBWriteBatch::Create() {
  return base::WrapUnique(new LevelDBWriteBatch);
}

LevelDBWriteBatch::LevelDBWriteBatch()
    : write_batch_(new leveldb::WriteBatch) {}

LevelDBWriteBatch::~LevelDBWriteBatch() {}

void LevelDBWriteBatch::Put(const base::StringPiece& key,
                            const base::StringPiece& value) {
  write_batch_->Put(leveldb_env::MakeSlice(key), leveldb_env::MakeSlice(value));
}

void LevelDBWriteBatch::Remove(const base::StringPiece& key) {
  write_batch_->Delete(leveldb_env::MakeSlice(key));
}

void LevelDBWriteBatch::Clear() {
  write_batch_->Clear();
}

}  // namespace content
