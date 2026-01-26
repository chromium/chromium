// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEGION_TESTING_FAKE_CONNECTION_H_
#define COMPONENTS_LEGION_TESTING_FAKE_CONNECTION_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/legion/connection.h"
#include "components/legion/error_code.h"
#include "components/legion/proto/legion.pb.h"

namespace legion {

class FakeConnection : public Connection {
 public:
  struct PendingRequest {
    PendingRequest();

    PendingRequest(PendingRequest&&);
    PendingRequest& operator=(PendingRequest&&);

    PendingRequest(const PendingRequest&) = delete;
    PendingRequest& operator=(const PendingRequest&) = delete;

    ~PendingRequest();

    proto::LegionRequest request;
    base::TimeDelta timeout;
    OnRequestCallback callback;
  };

  FakeConnection();
  ~FakeConnection() override;

  // Connection implementation:
  void Send(proto::LegionRequest request,
            base::TimeDelta timeout,
            OnRequestCallback callback) override;

  std::vector<PendingRequest>& pending_requests() { return pending_requests_; }

 private:
  std::vector<PendingRequest> pending_requests_;
};

}  // namespace legion

#endif  // COMPONENTS_LEGION_TESTING_FAKE_CONNECTION_H_
