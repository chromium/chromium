// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_CHAPS_UTIL_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_CHAPS_UTIL_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "crypto/scoped_nss_types.h"

namespace chromeos {

// Default implementation of the ChapsUtil class. Communicates with the chapsd
// daemon using ChapsSlotSession. Should be used on a worker thread.
// Exported for unit tests only.
class COMPONENT_EXPORT(CHAPS_UTIL) ChapsUtilImpl : public ChapsUtil {
 public:
  explicit ChapsUtilImpl(
      std::unique_ptr<ChapsSlotSessionFactory> chaps_slot_session_factory);
  ~ChapsUtilImpl() override;

  bool GenerateSoftwareBackedRSAKey(
      PK11SlotInfo* slot,
      uint16_t num_bits,
      crypto::ScopedSECKEYPublicKey* out_public_key,
      crypto::ScopedSECKEYPrivateKey* out_private_key) override;

  // If called with true, every slot is assumed to be a chaps-provided slot.
  void SetIsChapsProvidedSlotForTesting(
      bool is_chaps_provided_slot_for_testing) {
    is_chaps_provided_slot_for_testing_ = is_chaps_provided_slot_for_testing;
  }

 private:
  std::unique_ptr<ChapsSlotSession> GetChapsSlotSessionForSlot(
      PK11SlotInfo* slot);

  std::unique_ptr<ChapsSlotSessionFactory> const chaps_slot_session_factory_;

  // If true, every slot is assumed to be a chaps-provided slot.
  bool is_chaps_provided_slot_for_testing_;
};

}  // namespace chromeos

#endif  // CHROMEOS_ASH_COMPONENTS_CHAPS_UTIL_CHAPS_UTIL_IMPL_H_
