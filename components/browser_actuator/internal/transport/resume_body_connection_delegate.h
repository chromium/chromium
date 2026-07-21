// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_RESUME_BODY_CONNECTION_DELEGATE_H_
#define COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_RESUME_BODY_CONNECTION_DELEGATE_H_

#include <memory>

#include "base/functional/callback.h"
#include "components/browser_actuator/internal/transport/stream_connection_delegate.h"

namespace browser_actuator {

// Provides the resume body for a (re)connect request. It holds no sequence
// state of its own: a provider (granted by the channel) supplies the freshly
// serialized WatchSessionsRequest for each attempt, which the client uploads.
// This replaces the tracking DownstreamMessageConnectionDelegate — recording
// last-seen numbers now happens on the session, driven by the channel.
class ResumeBodyConnectionDelegate : public StreamConnectionDelegate {
 public:
  // Returns the serialized WatchSessionsRequest body for one attempt.
  using BodyProvider = base::RepeatingCallback<std::string()>;

  ResumeBodyConnectionDelegate(BodyProvider body_provider,
                               std::unique_ptr<StreamConnectionDelegate> inner);
  ~ResumeBodyConnectionDelegate() override;

  // StreamConnectionDelegate:
  void PrepareRequest(std::unique_ptr<network::ResourceRequest> request,
                      PrepareRequestCallback callback) override;
  void OnConnectionEstablished() override;
  bool ShouldRetryOnHttpFailure(int response_code) override;
  std::optional<StreamUploadBody> GetConnectionRequestBody() override;
  // OnMessageDispatched is intentionally left as the base no-op: resume
  // tracking moved to the session, so this delegate never inspects payloads.

 private:
  const BodyProvider body_provider_;
  std::unique_ptr<StreamConnectionDelegate> inner_;
};

}  // namespace browser_actuator

#endif  // COMPONENTS_BROWSER_ACTUATOR_INTERNAL_TRANSPORT_RESUME_BODY_CONNECTION_DELEGATE_H_
