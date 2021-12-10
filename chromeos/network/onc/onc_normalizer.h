// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_NORMALIZER_H_
#define CHROMEOS_NETWORK_ONC_ONC_NORMALIZER_H_

#include <memory>

#include "base/component_export.h"
#include "chromeos/components/onc/onc_mapper.h"

namespace chromeos {
namespace onc {

struct OncValueSignature;

class COMPONENT_EXPORT(CHROMEOS_NETWORK) Normalizer : public Mapper {
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
  std::unique_ptr<base::DictionaryValue> NormalizeObject(
      const OncValueSignature* object_signature,
      const base::Value& onc_object);

 private:
  // Dispatch to the right normalization function according to |signature|.
  std::unique_ptr<base::DictionaryValue> MapObject(
      const OncValueSignature& signature,
      const base::Value& onc_object,
      bool* error) override;

  void NormalizeCertificate(base::DictionaryValue* cert);
  void NormalizeEAP(base::DictionaryValue* eap);
  void NormalizeEthernet(base::DictionaryValue* ethernet);
  void NormalizeIPsec(base::DictionaryValue* ipsec);
  void NormalizeNetworkConfiguration(base::DictionaryValue* network);
  void NormalizeOpenVPN(base::DictionaryValue* openvpn);
  void NormalizeProxySettings(base::DictionaryValue* proxy);
  void NormalizeVPN(base::DictionaryValue* vpn);
  void NormalizeWiFi(base::DictionaryValue* wifi);
  void NormalizeStaticIPConfigForNetwork(base::DictionaryValue* network);

  const bool remove_recommended_fields_;
};

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_NORMALIZER_H_
