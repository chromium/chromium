// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_EMPTY_DM_TOKEN_RETRIEVER_H_
#define COMPONENTS_REPORTING_CLIENT_EMPTY_DM_TOKEN_RETRIEVER_H_

#include "components/reporting/client/dm_token_retriever.h"

namespace reporting {

// |EmptyDMTokenRetriever| is a |DMTokenRetriever| that is used for certain
// event types that do not need DM tokens to be retrieved or attached when
// creating a report queue. One such example is for device events, since device
// DM tokens are appended by default during event uploads.
class EmptyDMTokenRetriever : public DMTokenRetriever {
 public:
  EmptyDMTokenRetriever() = default;
  EmptyDMTokenRetriever(const EmptyDMTokenRetriever& other) = delete;
  EmptyDMTokenRetriever& operator=(const EmptyDMTokenRetriever& other) = delete;
  ~EmptyDMTokenRetriever() override = default;

  // Retrieves empty DM token and triggers the corresponding callback with the
  // result
  void RetrieveDMToken(
      DMTokenRetriever::CompletionCallback completion_cb) override;
};
}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_EMPTY_DM_TOKEN_RETRIEVER_H_
