// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ANDROID_EXPLICIT_PASSPHRASE_PLATFORM_CLIENT_H_
#define COMPONENTS_SYNC_ANDROID_EXPLICIT_PASSPHRASE_PLATFORM_CLIENT_H_

namespace syncer {

class SyncService;

// Shares the explicit passphrase content with layers outside of the browser
// which have an independent sync client, and thus separate encryption
// infrastructure. That way, if the user has entered their passphrase in the
// browser, it does not need to be entered again.
void SendExplicitPassphraseToJavaPlatformClient(
    const syncer::SyncService* sync_service);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ANDROID_EXPLICIT_PASSPHRASE_PLATFORM_CLIENT_H_
