// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_

#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/lens/contextual_input.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_search {

class MockContextualSearchSessionHandle : public ContextualSearchSessionHandle {
 public:
  MockContextualSearchSessionHandle();
  ~MockContextualSearchSessionHandle() override;

  MOCK_METHOD(void, NotifySessionStarted, (), (override));
  MOCK_METHOD(void,
              StartTabContextUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input_data,
               std::optional<lens::ImageEncodingOptions> image_options),
              (override));
  MOCK_METHOD(
      void,
      CreateSearchUrl,
      (std::unique_ptr<contextual_search::ContextualSearchContextController::
                           CreateSearchUrlRequestInfo> search_url_request_info,
       base::OnceCallback<void(GURL)> callback),
      (override));
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_SESSION_HANDLE_H_
