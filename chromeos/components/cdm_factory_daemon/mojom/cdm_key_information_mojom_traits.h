// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "chromeos/components/cdm_factory_daemon/mojom/content_decryption_module.mojom.h"
#include "media/base/cdm_key_information.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(CHROMEOS_CDM_MOJOM)
    EnumTraits<chromeos::cdm::mojom::CdmKeyStatus,
               ::media::CdmKeyInformation::KeyStatus> {
  static chromeos::cdm::mojom::CdmKeyStatus ToMojom(
      ::media::CdmKeyInformation::KeyStatus input);

  static bool FromMojom(chromeos::cdm::mojom::CdmKeyStatus input,
                        ::media::CdmKeyInformation::KeyStatus* output);
};

template <>
struct COMPONENT_EXPORT(CHROMEOS_CDM_MOJOM)
    StructTraits<chromeos::cdm::mojom::CdmKeyInformationDataView,
                 std::unique_ptr<media::CdmKeyInformation>> {
  static const std::vector<uint8_t>& key_id(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->key_id;
  }

  static media::CdmKeyInformation::KeyStatus status(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->status;
  }

  static uint32_t system_code(
      const std::unique_ptr<media::CdmKeyInformation>& input) {
    return input->system_code;
  }

  static bool Read(chromeos::cdm::mojom::CdmKeyInformationDataView input,
                   std::unique_ptr<media::CdmKeyInformation>* output);
};

}  // namespace mojo

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_MOJOM_CDM_KEY_INFORMATION_MOJOM_TRAITS_H_