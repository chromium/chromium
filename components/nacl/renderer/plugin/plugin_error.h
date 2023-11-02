// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Error codes and data structures used to report errors when loading a nexe.
 */

#ifndef COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_ERROR_H_
#define COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_ERROR_H_

#include <string>

#include "components/nacl/renderer/ppb_nacl_private.h"

namespace plugin {

class ErrorInfo {
 public:
  ErrorInfo() {
    SetReport(PP_NACL_ERROR_UNKNOWN, std::string());
  }

  ErrorInfo(const ErrorInfo&) = delete;
  ErrorInfo& operator=(const ErrorInfo&) = delete;

  void SetReport(PP_NaClError error_code, const std::string& message) {
    error_code_ = error_code;
    message_ = message;
  }

  PP_NaClError error_code() const {
    return error_code_;
  }

  const std::string& message() const {
    return message_;
  }

 private:
  PP_NaClError error_code_;
  std::string message_;
};

}  // namespace plugin

#endif  // COMPONENTS_NACL_RENDERER_PLUGIN_PLUGIN_ERROR_H_
