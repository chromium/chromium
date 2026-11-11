// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/mock_contextual_search_context_controller.h"

namespace contextual_search {

MockContextualSearchContextController::MockContextualSearchContextController() {
  ON_CALL(*this, MarkFileSuperceded)
      .WillByDefault(
          [this](const base::UnguessableToken& file_token) -> const FileInfo* {
            auto* file_info = const_cast<FileInfo*>(GetFileInfo(file_token));
            if (file_info) {
              file_info->is_superceded = true;
            }
            return file_info;
          });
}
MockContextualSearchContextController::
    ~MockContextualSearchContextController() = default;

}  // namespace contextual_search
