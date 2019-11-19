// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

This framework was once used to implement an asynchronous request/reply
protocol between the chrome://sync-internals page and the sync backend thread.
Much of it has been removed in favor of an ad-hoc system that allows us to
offer better safety guarantees, and to dispatch requests to different threads.

All that remains are some WeakHandles that allow us to send JsEvents from the
sync backend to about:sync.  The SyncInternalsUI implements JsEventHandler in
order to receive these events.  The SyncManager implements JsBackend in order
to send them.  The SyncJsController acts as an intermediary between them.

The old framework may still be useful to someone.  Feel free to retrieve it
from SVN history if you feel you can make use of it.
