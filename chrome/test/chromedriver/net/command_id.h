// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_COMMAND_ID_H_
#define CHROME_TEST_CHROMEDRIVER_NET_COMMAND_ID_H_

// ChromeDriver and Clients must use command ids to keep track of the commands
// being sent to DevTools. This static class contains logic for determining
// whether or not a command originated the client or chromedriver. This class
// is used in devtools_client_impl to determine log output and
// sync_websocket_impl to determine whether a received message should be
// processed to ChromeDriver or sent to the client.

class CommandId {
 public:
  // ChromeDriverCommandIds must be positive
  static bool IsChromeDriverCommandId(int command_id);

  // ClientCommandIds must be negative
  static bool IsClientCommandId(int command_id);
};

#endif  // CHROME_TEST_CHROMEDRIVER_NET_COMMAND_ID_H_
