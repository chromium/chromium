// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/renderer/queryable_data_store.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "base/values.h"
#include "chromecast/common/queryable_data.h"
#include "content/public/renderer/render_thread.h"

namespace chromecast {

QueryableDataStore::QueryableDataStore(
    const scoped_refptr<base::TaskRunner> render_main_thread)
    : render_main_thread_(std::move(render_main_thread)) {}

QueryableDataStore::~QueryableDataStore() {}

void QueryableDataStore::Set(const std::string& key, base::Value value) {
  render_main_thread_->PostTask(
      FROM_HERE, base::BindOnce(&QueryableData::RegisterQueryableValue, key,
                                std::move(value)));
}

void QueryableDataStore::BindQueryableDataStoreReceiver(
    mojo::PendingReceiver<shell::mojom::QueryableDataStore> receiver) {
  queryable_data_receivers_.Add(this, std::move(receiver));
}

}  // namespace chromecast
