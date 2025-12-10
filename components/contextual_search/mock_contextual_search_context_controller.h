// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_

#include <memory>
#include <optional>

#include "base/unguessable_token.h"
#include "components/contextual_search/contextual_search_context_controller.h"
#include "components/lens/contextual_input.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace contextual_search {

class MockContextualSearchContextController
    : public ContextualSearchContextController {
 public:
  MockContextualSearchContextController();
  ~MockContextualSearchContextController() override;

  MOCK_METHOD(void, InitializeIfNeeded, (), (override));
  MOCK_METHOD(
      GURL,
      CreateSearchUrl,
      (std::unique_ptr<CreateSearchUrlRequestInfo> search_url_request_info),
      (override));
  MOCK_METHOD(lens::ClientToAimMessage,
              CreateClientToAimRequest,
              (std::unique_ptr<CreateClientToAimRequestInfo>
                   create_client_to_aim_request_info),
              (override));
  MOCK_METHOD(void, AddObserver, (FileUploadStatusObserver * obs), (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (FileUploadStatusObserver * obs),
              (override));
  MOCK_METHOD(void,
              StartFileUploadFlow,
              (const base::UnguessableToken& file_token,
               std::unique_ptr<lens::ContextualInputData> contextual_input_data,
               std::optional<lens::ImageEncodingOptions> image_options),
              (override));
  MOCK_METHOD(bool,
              DeleteFile,
              (const base::UnguessableToken& file_token),
              (override));
  MOCK_METHOD(void, ClearFiles, (), (override));
  MOCK_METHOD(
      std::unique_ptr<lens::proto::LensOverlaySuggestInputs>,
      CreateSuggestInputs,
      (const std::vector<base::UnguessableToken>& attached_context_tokens),
      (override));
  MOCK_METHOD(const FileInfo*,
              GetFileInfo,
              (const base::UnguessableToken& file_token),
              (override));
  MOCK_METHOD(std::vector<const FileInfo*>, GetFileInfoList, (), (override));
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_MOCK_CONTEXTUAL_SEARCH_CONTEXT_CONTROLLER_H_
