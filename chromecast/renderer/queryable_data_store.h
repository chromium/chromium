// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_QUERYABLE_DATA_STORE_H_
#define CHROMECAST_RENDERER_QUERYABLE_DATA_STORE_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/common/mojom/queryable_data_store.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace base {
class Value;
class TaskRunner;
}  // namespace base

namespace chromecast {

// This class is used to sync queryable data on the renderer thread based on
// messages from the browser process.
class QueryableDataStore : public shell::mojom::QueryableDataStore {
 public:
  explicit QueryableDataStore(
      const scoped_refptr<base::TaskRunner> render_main_thread);
  ~QueryableDataStore() override;

  void BindQueryableDataStoreReceiver(
      mojo::PendingReceiver<shell::mojom::QueryableDataStore> receiver);

 private:
  // shell::mojom::QueryableDataStore implementation:
  void Set(const std::string& key, base::Value value) override;

  const scoped_refptr<base::TaskRunner> render_main_thread_;

  mojo::ReceiverSet<shell::mojom::QueryableDataStore> queryable_data_receivers_;

  DISALLOW_COPY_AND_ASSIGN(QueryableDataStore);
};

}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_QUERYABLE_DATA_STORE_H_
