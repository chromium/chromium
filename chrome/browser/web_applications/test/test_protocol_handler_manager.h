// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PROTOCOL_HANDLER_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PROTOCOL_HANDLER_MANAGER_H_

#include "chrome/browser/web_applications/components/protocol_handler_manager.h"

namespace web_app {

// testing implementation of ProtocolHandlerManager
class TestProtocolHandlerManager : public ProtocolHandlerManager {
 public:
  explicit TestProtocolHandlerManager(Profile* profile);
  ~TestProtocolHandlerManager() override;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_TEST_PROTOCOL_HANDLER_MANAGER_H_
