// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_

#include <memory>
#include <string>

namespace google::protobuf {
class MessageLite;
}  // namespace google::protobuf

namespace ash::babelorca {

struct RequestDataWrapper;

class TachyonAuthedClient {
 public:
  TachyonAuthedClient(const TachyonAuthedClient&) = delete;
  TachyonAuthedClient& operator=(const TachyonAuthedClient&) = delete;

  virtual ~TachyonAuthedClient() = default;

  virtual void StartAuthedRequest(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::unique_ptr<google::protobuf::MessageLite> request_proto) = 0;

  virtual void StartAuthedRequestString(
      std::unique_ptr<RequestDataWrapper> request_data,
      std::string request_string) = 0;

 protected:
  TachyonAuthedClient() = default;
};

}  // namespace ash::babelorca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_BABELORCA_TACHYON_AUTHED_CLIENT_H_
