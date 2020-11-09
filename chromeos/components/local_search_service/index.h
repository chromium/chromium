// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_
#define CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_

#include <vector>

#include "base/strings/string16.h"
#include "chromeos/components/local_search_service/public/mojom/index.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace chromeos {
namespace local_search_service {

class Index : public mojom::Index {
 public:
  explicit Index(IndexId index_id, Backend backend);
  ~Index() override;

  void BindReceiver(mojo::PendingReceiver<mojom::Index> receiver);

 private:
  mojo::ReceiverSet<mojom::Index> receivers_;
};

}  // namespace local_search_service
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_LOCAL_SEARCH_SERVICE_INDEX_H_
