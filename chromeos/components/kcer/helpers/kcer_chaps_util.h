// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_KCER_HELPERS_KCER_CHAPS_UTIL_H_
#define CHROMEOS_COMPONENTS_KCER_HELPERS_KCER_CHAPS_UTIL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "chromeos/ash/components/chaps_util/chaps_slot_session.h"
#include "chromeos/ash/components/chaps_util/chaps_util.h"
#include "chromeos/components/kcer/helpers/pkcs12_reader.h"
#include "crypto/scoped_nss_types.h"

namespace kcer::internal {

// Class helper for importing pkcs12 containers to Chaps, very similar to
// ChapsUtil class. Communicates with the chapsd daemon using ChapsSlotSession.
// Should be used on a worker thread. Exported for unit tests only.
class COMPONENT_EXPORT(KCER) KcerChapsUtil {
 public:
  explicit KcerChapsUtil(std::unique_ptr<chromeos::ChapsSlotSessionFactory>
                             chaps_slot_session_factory);
  ~KcerChapsUtil();

  static std::unique_ptr<KcerChapsUtil> Create();
  using FactoryCallback =
      base::RepeatingCallback<std::unique_ptr<KcerChapsUtil>()>;

  static void SetFactoryForTesting(const FactoryCallback& factory);

  bool ImportPkcs12Certificate(PK11SlotInfo* slot,
                               const std::vector<uint8_t>& pkcs12_data,
                               const std::string& password,
                               bool is_software_backed);

  // Public for testing, allows replacing Pkcs12Reader.
  bool ImportPkcs12CertificateImpl(
      PK11SlotInfo* slot,
      const std::vector<uint8_t>& pkcs12_data,
      const std::string& password,
      const bool is_software_backed,
      const Pkcs12Reader& pkcs12_reader = Pkcs12Reader());

  // If called with true, every slot is assumed to be a chaps-provided slot.
  void SetIsChapsProvidedSlotForTesting(
      bool is_chaps_provided_slot_for_testing) {
    is_chaps_provided_slot_for_testing_ = is_chaps_provided_slot_for_testing;
  }

 private:
  std::unique_ptr<chromeos::ChapsSlotSession> GetChapsSlotSessionForSlot(
      PK11SlotInfo* slot);

  std::unique_ptr<chromeos::ChapsSlotSessionFactory> const
      chaps_slot_session_factory_;

  // If true, every slot is assumed to be a chaps-provided slot.
  bool is_chaps_provided_slot_for_testing_;
};

}  // namespace kcer::internal

#endif  // CHROMEOS_COMPONENTS_KCER_HELPERS_KCER_CHAPS_UTIL_H_
