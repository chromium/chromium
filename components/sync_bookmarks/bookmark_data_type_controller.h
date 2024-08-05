// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace sync_bookmarks {

class BookmarkDataTypeController : public syncer::DataTypeController {
 public:
  BookmarkDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      std::unique_ptr<syncer::DataTypeLocalDataBatchUploader> batch_uploader);

  BookmarkDataTypeController(const BookmarkDataTypeController&) = delete;
  BookmarkDataTypeController& operator=(const BookmarkDataTypeController&) =
      delete;

  ~BookmarkDataTypeController() override;
};

}  // namespace sync_bookmarks

#endif  // COMPONENTS_SYNC_BOOKMARKS_BOOKMARK_DATA_TYPE_CONTROLLER_H_
