// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_NORMALIZER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_NORMALIZER_H_

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_mapper.h"

namespace chromeos::onc {
struct OncValueSignature;
}

namespace ash::onc {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) Normalizer
    : public chromeos::onc::Mapper {
 public:
  explicit Normalizer(bool remove_recommended_fields);

  Normalizer(const Normalizer&) = delete;
  Normalizer& operator=(const Normalizer&) = delete;

  ~Normalizer() override;

  // Removes all fields that are ignored/irrelevant because of the value of
  // other fields. E.g. the "WiFi" field is irrelevant if the configurations
  // type is "Ethernet". If |remove_recommended_fields| is true, kRecommended
  // arrays are removed (the array itself and not the field names listed
  // there). |object_signature| must point to one of the signatures in
  // |onc_signature.h|. For configurations of type "WiFi", if the "SSID" field
  // is set, but the field "HexSSID" is not, the contents of the "SSID" field is
  // converted to UTF-8 encoding, a hex representation of the byte sequence is
  // created and stored in the field "HexSSID".
  base::DictValue NormalizeObject(
      const chromeos::onc::OncValueSignature* object_signature,
      const base::DictValue& onc_object);

 private:
  // Dispatch to the right normalization function according to |signature|.
  base::DictValue MapObject(const chromeos::onc::OncValueSignature& signature,
                            const base::DictValue& onc_object,
                            bool* error) override;

  void NormalizeCertificate(base::DictValue* cert);
  void NormalizeEAP(base::DictValue* eap);
  void NormalizeEthernet(base::DictValue* ethernet);
  void NormalizeIPsec(base::DictValue* ipsec);
  void NormalizeNetworkConfiguration(base::DictValue* network);
  void NormalizeOpenVPN(base::DictValue* openvpn);
  void NormalizeProxySettings(base::DictValue* proxy);
  void NormalizeVPN(base::DictValue* vpn);
  void NormalizeWiFi(base::DictValue* wifi);
  void NormalizeStaticIPConfigForNetwork(base::DictValue* network);

  const bool remove_recommended_fields_;
};

}  // namespace ash::onc

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_ONC_ONC_NORMALIZER_H_
