// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/public/persistent_key_value_store.h"

namespace feed {

PersistentKeyValueStore::Result::Result() = default;
PersistentKeyValueStore::Result::Result(Result&&) = default;
PersistentKeyValueStore::Result& PersistentKeyValueStore::Result::operator=(
    Result&&) = default;
PersistentKeyValueStore::Result::~Result() = default;

}  // namespace feed
