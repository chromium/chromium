// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/mojom/cdm_key_information_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

using MojomKeyStatus = chromeos::cdm::mojom::CdmKeyStatus;
using NativeKeyStatus = media::CdmKeyInformation::KeyStatus;

// static
MojomKeyStatus EnumTraits<MojomKeyStatus, NativeKeyStatus>::ToMojom(
    NativeKeyStatus input) {
  switch (input) {
    case NativeKeyStatus::USABLE:
      return MojomKeyStatus::USABLE;
    case NativeKeyStatus::INTERNAL_ERROR:
      return MojomKeyStatus::INTERNAL_ERROR;
    case NativeKeyStatus::EXPIRED:
      return MojomKeyStatus::EXPIRED;
    case NativeKeyStatus::OUTPUT_RESTRICTED:
      return MojomKeyStatus::OUTPUT_RESTRICTED;
    case NativeKeyStatus::OUTPUT_DOWNSCALED:
      return MojomKeyStatus::OUTPUT_DOWNSCALED;
    case NativeKeyStatus::KEY_STATUS_PENDING:
      return MojomKeyStatus::KEY_STATUS_PENDING;
    case NativeKeyStatus::RELEASED:
      return MojomKeyStatus::RELEASED;
    case NativeKeyStatus::USABLE_IN_FUTURE:
      return MojomKeyStatus::USABLE_IN_FUTURE;
  }
  NOTREACHED();
}

// static
std::optional<NativeKeyStatus>
EnumTraits<MojomKeyStatus, NativeKeyStatus>::FromMojom(MojomKeyStatus input) {
  switch (input) {
    case MojomKeyStatus::USABLE:
      return NativeKeyStatus::USABLE;
    case MojomKeyStatus::INTERNAL_ERROR:
      return NativeKeyStatus::INTERNAL_ERROR;
    case MojomKeyStatus::EXPIRED:
      return NativeKeyStatus::EXPIRED;
    case MojomKeyStatus::OUTPUT_RESTRICTED:
      return NativeKeyStatus::OUTPUT_RESTRICTED;
    case MojomKeyStatus::OUTPUT_DOWNSCALED:
      return NativeKeyStatus::OUTPUT_DOWNSCALED;
    case MojomKeyStatus::KEY_STATUS_PENDING:
      return NativeKeyStatus::KEY_STATUS_PENDING;
    case MojomKeyStatus::RELEASED:
      return NativeKeyStatus::RELEASED;
    case MojomKeyStatus::USABLE_IN_FUTURE:
      return NativeKeyStatus::USABLE_IN_FUTURE;
  }
  NOTREACHED();
}

// static
bool StructTraits<chromeos::cdm::mojom::CdmKeyInformationDataView,
                  std::unique_ptr<media::CdmKeyInformation>>::
    Read(chromeos::cdm::mojom::CdmKeyInformationDataView input,
         std::unique_ptr<media::CdmKeyInformation>* output) {
  mojo::ArrayDataView<uint8_t> key_id;
  input.GetKeyIdDataView(&key_id);

  NativeKeyStatus status;
  if (!input.ReadStatus(&status))
    return false;

  *output = std::make_unique<media::CdmKeyInformation>(key_id, status,
                                                       input.system_code());
  return true;
}

}  // namespace mojo
