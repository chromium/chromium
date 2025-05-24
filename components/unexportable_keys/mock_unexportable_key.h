// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_H_

#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

class MockUnexportableKey : public crypto::UnexportableSigningKey {
 public:
  MockUnexportableKey();
  ~MockUnexportableKey() override;

  // crypto::UnexportableSigningKey:
  MOCK_METHOD(crypto::SignatureVerifier::SignatureAlgorithm,
              Algorithm,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>,
              GetSubjectPublicKeyInfo,
              (),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>, GetWrappedKey, (), (const, override));
  MOCK_METHOD(std::optional<std::vector<uint8_t>>,
              SignSlowly,
              (base::span<const uint8_t> data),
              (override));
  MOCK_METHOD(bool, IsHardwareBacked, (), (const, override));
#if BUILDFLAG(IS_MAC)
  MOCK_METHOD(SecKeyRef, GetSecKeyRef, (), (const, override));
#endif  // BUILDFLAG(IS_MAC)
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_H_
