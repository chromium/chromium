// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_
#define CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"

class SyncWebSocket;
class URLRequestContextGetter;

typedef base::RepeatingCallback<std::unique_ptr<SyncWebSocket>()>
    SyncWebSocketFactory;

SyncWebSocketFactory CreateSyncWebSocketFactory(
    URLRequestContextGetter* getter);

#endif  // CHROME_TEST_CHROMEDRIVER_NET_SYNC_WEBSOCKET_FACTORY_H_
