// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_H_
#define COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_H_

#include <string>

#include "components/sync/base/model_type.h"

namespace sync_pb {
class NigoriSpecifics;
}

namespace syncer {

class Cryptographer;
class DirectoryCryptographer;
enum class PassphraseType;

namespace syncable {

class BaseTransaction;

// Sync internal interface for dealing with nigori node and querying
// the current set of encrypted types. Not thread safe, so a sync transaction
// must be held by a caller whenever invoking methods.
class NigoriHandler {
 public:
  NigoriHandler();
  virtual ~NigoriHandler();

  // Apply a nigori node update, updating the internal encryption state
  // accordingly. Returns true in case of success, or false if the update has
  // been ignored.
  virtual bool ApplyNigoriUpdate(const sync_pb::NigoriSpecifics& nigori,
                                 syncable::BaseTransaction* const trans) = 0;

  // Store the current encrypt everything/encrypted types state into |nigori|.
  virtual void UpdateNigoriFromEncryptedTypes(
      sync_pb::NigoriSpecifics* nigori,
      const syncable::BaseTransaction* const trans) const = 0;

  // Returns the original cryptographer.
  virtual const Cryptographer* GetCryptographer(
      const syncable::BaseTransaction* const trans) const = 0;

  // Returns the full-blown DirectoryCryptographer API, available only if the
  // legacy directory-based implementation of NIGORI is active.
  virtual const DirectoryCryptographer* GetDirectoryCryptographer(
      const syncable::BaseTransaction* const trans) const = 0;

  // Returns the set of currently encrypted types.
  virtual ModelTypeSet GetEncryptedTypes(
      const syncable::BaseTransaction* const trans) const = 0;

  // Returns current value for the passphrase type.
  virtual PassphraseType GetPassphraseType(
      const syncable::BaseTransaction* const trans) const = 0;
};

}  // namespace syncable
}  // namespace syncer

#endif  // COMPONENTS_SYNC_SYNCABLE_NIGORI_HANDLER_H_
