// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_
#define COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/ownership/ownership_export.h"
#include "crypto/scoped_nss_types.h"

struct PK11SlotInfoStr;
typedef struct PK11SlotInfoStr PK11SlotInfo;

namespace ownership {

// This class is a ref-counted wrapper around a plain public key.
class OWNERSHIP_EXPORT PublicKey
    : public base::RefCountedThreadSafe<PublicKey> {
 public:
  PublicKey();

  PublicKey(const PublicKey&) = delete;
  PublicKey& operator=(const PublicKey&) = delete;

  std::vector<uint8_t>& data() { return data_; }

  bool is_loaded() const { return !data_.empty(); }

  std::string as_string() {
    return std::string(reinterpret_cast<const char*>(data_.data()),
                       data_.size());
  }

 private:
  friend class base::RefCountedThreadSafe<PublicKey>;

  virtual ~PublicKey();

  std::vector<uint8_t> data_;
};

// This class is a ref-counted wrapper around a SECKEYPrivateKey
// instance.
class OWNERSHIP_EXPORT PrivateKey
    : public base::RefCountedThreadSafe<PrivateKey> {
 public:
  explicit PrivateKey(crypto::ScopedSECKEYPrivateKey key);

  PrivateKey(const PrivateKey&) = delete;
  PrivateKey& operator=(const PrivateKey&) = delete;

  SECKEYPrivateKey* key() { return key_.get(); }

 private:
  friend class base::RefCountedThreadSafe<PrivateKey>;

  virtual ~PrivateKey();

  crypto::ScopedSECKEYPrivateKey key_;
};

// This class is a helper class that allows to import public/private
// parts of the owner key.
class OWNERSHIP_EXPORT OwnerKeyUtil
    : public base::RefCountedThreadSafe<OwnerKeyUtil> {
 public:
  // Attempts to read the public key from the file system.  Upon success,
  // returns true and populates |output|.  False on failure.
  virtual bool ImportPublicKey(std::vector<uint8_t>* output) = 0;

  // Looks for the private key associated with |key| in the |slot|
  // and returns it if it can be found.  Returns NULL otherwise.
  // Caller takes ownership.
  virtual crypto::ScopedSECKEYPrivateKey FindPrivateKeyInSlot(
      const std::vector<uint8_t>& key,
      PK11SlotInfo* slot) = 0;

  // Checks whether the public key is present in the file system.
  virtual bool IsPublicKeyPresent() = 0;

 protected:
  virtual ~OwnerKeyUtil() {}

 private:
  friend class base::RefCountedThreadSafe<OwnerKeyUtil>;
};

}  // namespace ownership

#endif  // COMPONENTS_OWNERSHIP_OWNER_KEY_UTIL_H_
