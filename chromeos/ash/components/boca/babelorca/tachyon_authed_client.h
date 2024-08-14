// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_

#include <memory>
#include <string>
#include <string_view>

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash::babelorca {

class ResponseCallbackWrapper;

class TachyonAuthedClient {
 public:
  TachyonAuthedClient(const TachyonAuthedClient&) = delete;
  TachyonAuthedClient& operator=(const TachyonAuthedClient&) = delete;

  virtual ~TachyonAuthedClient() = default;

  virtual void StartAuthedRequest(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::unique_ptr<google::protobuf::MessageLite> request_proto,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) = 0;

  virtual void StartAuthedRequestString(
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      std::string request_string,
      std::string_view url,
      int max_retries,
      std::unique_ptr<ResponseCallbackWrapper> response_cb) = 0;

 protected:
  TachyonAuthedClient() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_
