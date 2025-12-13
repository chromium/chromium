// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_CONNECTION_INFO_POLLER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_CONNECTION_INFO_POLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace ash::boca {
template <class T>
class RetriableRequestSender;
}  // namespace ash::boca

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace boca {
class KioskReceiverConnection;
}  // namespace boca

namespace ash::boca_receiver {

class ReceiverConnectionInfoPoller {
 public:
  using OnStopCallback = base::OnceCallback<void(bool server_unreachable)>;

  ReceiverConnectionInfoPoller();

  ReceiverConnectionInfoPoller(const ReceiverConnectionInfoPoller&) = delete;
  ReceiverConnectionInfoPoller& operator=(const ReceiverConnectionInfoPoller&) =
      delete;

  ~ReceiverConnectionInfoPoller();

  void Start(const std::string& receiver_id,
             const std::string& connection_id,
             std::unique_ptr<google_apis::RequestSender> request_sender,
             OnStopCallback on_stop_callback);

  void Stop();

 private:
  void PollConnectionInfo(const std::string& receiver_id,
                          const std::string& connection_id,
                          OnStopCallback on_stop_callback);
  void OnConnectionInfoPolled(
      const std::string& receiver_id,
      const std::string& connection_id,
      OnStopCallback on_stop_callback,
      std::optional<::boca::KioskReceiverConnection> response);

  std::unique_ptr<boca::RetriableRequestSender<::boca::KioskReceiverConnection>>
      retriable_sender_;
  base::OneShotTimer polling_timer_;
  int consecutive_failure_count_ = 0;

  base::WeakPtrFactory<ReceiverConnectionInfoPoller> weak_ptr_factory_{this};
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_CONNECTION_INFO_POLLER_H_
