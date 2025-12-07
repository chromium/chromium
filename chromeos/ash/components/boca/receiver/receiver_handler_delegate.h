// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_H_

#include <memory>
#include <string_view>

namespace ash::boca {
class FCMHandler;
class SpotlightRemotingClientManager;
}  // namespace ash::boca

namespace google_apis {
class RequestSender;
}  // namespace google_apis

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::boca_receiver {

class ReceiverHandlerDelegate {
 public:
  ReceiverHandlerDelegate(const ReceiverHandlerDelegate&) = delete;
  ReceiverHandlerDelegate& operator=(const ReceiverHandlerDelegate&) = delete;

  virtual ~ReceiverHandlerDelegate() = default;

  virtual boca::FCMHandler* GetFcmHandler() const = 0;

  virtual std::unique_ptr<google_apis::RequestSender> CreateRequestSender(
      std::string_view requester_id,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) const = 0;

  virtual boca::SpotlightRemotingClientManager* GetRemotingClient() const = 0;

  virtual bool IsAppEnabled(std::string_view url) = 0;

 protected:
  ReceiverHandlerDelegate() = default;
};

}  // namespace ash::boca_receiver

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_RECEIVER_RECEIVER_HANDLER_DELEGATE_H_
