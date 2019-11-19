// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_DATA_TYPE_ENCRYPTION_HANDLER_H_
#define COMPONENTS_SYNC_DRIVER_DATA_TYPE_ENCRYPTION_HANDLER_H_

#include "components/sync/base/model_type.h"

namespace syncer {

// The DataTypeEncryptionHandler provides the status of datatype encryption.
class DataTypeEncryptionHandler {
 public:
  DataTypeEncryptionHandler();
  virtual ~DataTypeEncryptionHandler();

  // Returns whether there is an error that prevents encryption or decryption
  // from proceeding. This does not necessarily mean that the UI will display an
  // error state, for example if there's a user-transparent attempt to resolve
  // the crypto error.
  virtual bool HasCryptoError() const = 0;

  // Returns the current set of encrypted data types.
  virtual ModelTypeSet GetEncryptedDataTypes() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_DATA_TYPE_ENCRYPTION_HANDLER_H_
