// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/management_ui_handler_chromeos.h"

net::NetworkTrafficAnnotationTag GetManagementUICustomerLogoAnnotation() {
  return net::DefineNetworkTrafficAnnotation("management_ui_customer_logo", R"(
      semantics {
        sender: "Management UI Handler"
        description:
          "Download organization logo for visualization on the "
          "chrome://management page."
        trigger:
          "The user managed by organization that provides a company logo "
          "in their GSuites account loads the chrome://management page."
        data:
          "Organization uploaded image URL."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "This feature cannot be disabled by settings, but it is only "
          "triggered by a user action."
        policy_exception_justification: "Not implemented."
      })");
}
