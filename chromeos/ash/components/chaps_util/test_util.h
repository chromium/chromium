// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_TEST_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_TEST_UTIL_H_

#include <pk11pub.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "crypto/scoped_nss_types.h"

namespace chromeos {

// A fake implementation of ChapsUtil which actually just generates a key pair
// through NSS.
class FakeChapsUtil : public ChapsUtil {
 public:
  using OnKeyGenerated = base::RepeatingCallback<void(const std::string& spki)>;

  explicit FakeChapsUtil(OnKeyGenerated on_key_generated);
  ~FakeChapsUtil() override;

  bool GenerateSoftwareBackedRSAKey(
      PK11SlotInfo* slot,
      uint16_t num_bits,
      crypto::ScopedSECKEYPublicKey* out_public_key,
      crypto::ScopedSECKEYPrivateKey* out_private_key) override;

 private:
  OnKeyGenerated on_key_generated_;
};

// While an instance of this class exists, ChapsUtil::Create() will return
// instances of FakeChapsUtil (see above). Only one instance of this class
// should exist at a time.
class ScopedChapsUtilOverride {
 public:
  // Sets up ChapsUtil to return instances of FakeChapsUtil (see above) from
  // ChapsUtil::Create().
  ScopedChapsUtilOverride();
  // Reverts the changes performed by the constructor.
  ~ScopedChapsUtilOverride();

  // Returns all der-encoded SPKIs that were generated through ChapsUtil
  // instances returned from ChapsUtil::Create() while this override was active,
  // i.e. since this instances has been constructed.
  const std::vector<std::string>& generated_key_spkis() {
    return generated_key_spkis_;
  }

 private:
  std::unique_ptr<ChapsUtil> CreateChapsUtil();

  // Called when a FakeChapsUtil instance created by CreateChapsUtil generates a
  // key pair.
  void OnKeyGenerated(const std::string& spki);

  // Tracks key pairs that were generated through FakeChapsUtil instances
  // created by CreateChapsUtil().
  std::vector<std::string> generated_key_spkis_;

  base::WeakPtrFactory<ScopedChapsUtilOverride> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_TEST_UTIL_H_
