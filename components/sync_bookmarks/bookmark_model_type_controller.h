// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/service/model_type_controller.h"

namespace sync_bookmarks {

class BookmarkModelTypeController : public syncer::ModelTypeController {
 public:
  BookmarkModelTypeController(
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode);

  BookmarkModelTypeController(const BookmarkModelTypeController&) = delete;
  BookmarkModelTypeController& operator=(const BookmarkModelTypeController&) =
      delete;

  ~BookmarkModelTypeController() override;

  // DataTypeController overrides.
  bool ShouldRunInTransportOnlyMode() const override;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_MODEL_TYPE_CONTROLLER_H_
