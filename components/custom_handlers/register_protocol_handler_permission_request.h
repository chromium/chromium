// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_
#define COMPONENTS_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/permissions/permission_request.h"

namespace permissions {
enum class RequestType;
}  // namespace permissions

class GURL;
namespace custom_handlers {
class ProtocolHandlerRegistry;

// This class provides display data for a permission request, shown when a page
// wants to register a protocol handler and was triggered by a user action.
class RegisterProtocolHandlerPermissionRequest
    : public permissions::PermissionRequest {
 public:
  RegisterProtocolHandlerPermissionRequest(
      custom_handlers::ProtocolHandlerRegistry* registry,
      const ProtocolHandler& handler,
      GURL url,
      base::ScopedClosureRunner fullscreen_block);

  RegisterProtocolHandlerPermissionRequest(
      const RegisterProtocolHandlerPermissionRequest&) = delete;
  RegisterProtocolHandlerPermissionRequest& operator=(
      const RegisterProtocolHandlerPermissionRequest&) = delete;

  ~RegisterProtocolHandlerPermissionRequest() override;

 private:
  // permissions::PermissionRequest:
  bool IsDuplicateOf(
      permissions::PermissionRequest* other_request) const override;
  std::u16string GetMessageTextFragment() const override;

  void PermissionDecided(ContentSetting result,
                         bool is_one_time,
                         bool is_final_decision);
  void DeleteRequest();

  raw_ptr<custom_handlers::ProtocolHandlerRegistry> registry_;
  ProtocolHandler handler_;
  // Fullscreen will be blocked for the duration of the lifetime of this block.
  // TODO(avi): Move to either permissions::PermissionRequest or the
  // PermissionRequestManager?
  base::ScopedClosureRunner fullscreen_block_;
};

}  // namespace custom_handlers

#endif  // COMPONENTS_CUSTOM_HANDLERS_REGISTER_PROTOCOL_HANDLER_PERMISSION_REQUEST_H_
