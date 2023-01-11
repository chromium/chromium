// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_CLIENT_DM_TOKEN_RETRIEVER_H_
#define COMPONENTS_REPORTING_CLIENT_DM_TOKEN_RETRIEVER_H_

#include <string>

#include "base/functional/callback.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

// A |DMTokenRetriever| is used to retrieve DM tokens and is used by the
// |ReportQueueProvider| to autonomously retrieve these tokens as necessary
// while building a report queue.
class DMTokenRetriever {
 public:
  // Callback triggered once the DM token has been retrieved with the
  // corresponding result
  using CompletionCallback = base::OnceCallback<void(StatusOr<std::string>)>;

  DMTokenRetriever() = default;
  DMTokenRetriever(const DMTokenRetriever& other) = delete;
  DMTokenRetriever& operator=(const DMTokenRetriever& other) = delete;
  virtual ~DMTokenRetriever() = default;

  virtual void RetrieveDMToken(CompletionCallback completion_cb) = 0;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_CLIENT_DM_TOKEN_RETRIEVER_H_
