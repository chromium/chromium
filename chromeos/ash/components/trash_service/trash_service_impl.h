// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_TRASH_SERVICE_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_TRASH_SERVICE_IMPL_H_

#include <stdint.h>

#include "base/files/file.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace ash::trash_service {

// Implementation of the Trash utility process. The trash implementation follows
// the XDG specification which maintains metadata in a user visible directory at
// ({TRASH_DIR}/info) and in files with a suffix .trashinfo. These files have a
// format as follows:
//   [Trash Info]
//   Path=/foo/bar.txt
//   DeletionDate=2011-10-05T14:48:00.000Z
// Where Path is relative to the parent of the {TRASH_DIR} and the DeletionDate
// is an ISO-8601 compliant string. This utility process parses this content and
// ensures both these values are well-formed and valid. Upon completion it
// responds with a status and the parsed contents.
class TrashServiceImpl : public mojom::TrashService {
 public:
  explicit TrashServiceImpl(
      mojo::PendingReceiver<mojom::TrashService> receiver);
  ~TrashServiceImpl() override;

  TrashServiceImpl(const TrashServiceImpl&) = delete;
  TrashServiceImpl& operator=(const TrashServiceImpl&) = delete;

  // mojom::TrashService:
  void ParseTrashInfoFile(base::File trash_info_file,
                          ParseTrashInfoFileCallback callback) override;

 private:
  mojo::ReceiverSet<mojom::TrashService> receivers_;
};

}  // namespace ash::trash_service

#endif  // CHROMEOS_ASH_COMPONENTS_TRASH_SERVICE_TRASH_SERVICE_IMPL_H_
