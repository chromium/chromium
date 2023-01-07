// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SERVER_NIGORI_HELPER_H_
#define COMPONENTS_SYNC_TEST_FAKE_SERVER_NIGORI_HELPER_H_

namespace sync_pb {
class NigoriSpecifics;
}  // namespace sync_pb

namespace fake_server {

class FakeServer;

// Given a |fake_server|, fetches its Nigori node and writes it to the
// proto pointed to by |nigori|. Returns false if the server does not contain
// exactly one Nigori node.
bool GetServerNigori(FakeServer* fake_server, sync_pb::NigoriSpecifics* nigori);

// Given a |fake_server|, sets the Nigori instance stored in it to |nigori|.
void SetNigoriInFakeServer(const sync_pb::NigoriSpecifics& nigori,
                           FakeServer* fake_server);

// Given a |fake_server|, sets the Nigori instance stored in it to a standard
// Keystore Nigori. |fake_server| must contain single keystore key.
void SetKeystoreNigoriInFakeServer(FakeServer* fake_server);

}  // namespace fake_server

#endif  // COMPONENTS_SYNC_TEST_FAKE_SERVER_NIGORI_HELPER_H_
