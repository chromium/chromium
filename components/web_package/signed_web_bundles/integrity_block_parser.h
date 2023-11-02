// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_

#include "components/web_package/web_bundle_parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_package {

// A parser for a signed bundle's metadata. This class owns itself and will self
// destruct after calling the ParseIntergrityBlockCallback.
class IntegrityBlockParser : WebBundleParser::SharedBundleDataSource::Observer {
 public:
  IntegrityBlockParser(
      scoped_refptr<WebBundleParser::SharedBundleDataSource> data_source,
      WebBundleParser::ParseIntegrityBlockCallback callback);

  IntegrityBlockParser(const IntegrityBlockParser&) = delete;
  IntegrityBlockParser& operator=(const IntegrityBlockParser&) = delete;

  ~IntegrityBlockParser() override;

  void Start();

  // CBOR of the bytes present at the start of the Signed Web Bundle, including
  // the magic string "🖋📦".
  //
  // The first 10 bytes of the integrity block format are:
  //   83                             -- Array of length 3
  //      48                          -- Byte string of length 8
  //         F0 9F 96 8B F0 9F 93 A6  -- "🖋📦" in UTF-8
  // Note: The length of the top level array is 3 (magic, version, signature
  // stack).
  static constexpr uint8_t kIntegrityBlockMagicBytes[] = {
      0x83, 0x48,
      // "🖋📦" magic bytes
      0xF0, 0x9F, 0x96, 0x8B, 0xF0, 0x9F, 0x93, 0xA6};

  // CBOR of the version string "1b\0\0".
  //   44               -- Byte string of length 4
  //       31 62 00 00  -- "1b\0\0"
  static constexpr uint8_t kIntegrityBlockVersionMagicBytes[] = {
      0x44, '1', 'b', 0x00, 0x00,
  };

 private:
  void ParseMagicBytesAndVersion(
      const absl::optional<std::vector<uint8_t>>& data);

  void ParseSignatureStack(uint64_t offset_in_stream,
                           const absl::optional<std::vector<uint8_t>>& data);

  void ReadSignatureStackEntry(const uint64_t offset_in_stream,
                               const uint64_t signature_stack_entries_left);

  void ParseSignatureStackEntry(
      uint64_t offset_in_stream,
      const uint64_t signature_stack_entries_left,
      const absl::optional<std::vector<uint8_t>>& data);

  void ParseSignatureStackEntryAttributesHeader(
      uint64_t offset_in_stream,
      const uint64_t signature_stack_entries_left,
      mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
      const absl::optional<std::vector<uint8_t>>& data);

  void ParseSignatureStackEntryAttributesPublicKeyKey(
      uint64_t offset_in_stream,
      const uint64_t signature_stack_entries_left,
      mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
      const absl::optional<std::vector<uint8_t>>& data);

  void ReadSignatureStackEntryAttributesPublicKeyValue(
      uint64_t offset_in_stream,
      const uint64_t signature_stack_entries_left,
      mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
      const absl::optional<std::vector<uint8_t>>& public_key);

  void ParseSignatureStackEntrySignatureHeader(
      uint64_t offset_in_stream,
      const uint64_t signature_stack_entries_left,
      mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
      const absl::optional<std::vector<uint8_t>>& data);

  void ParseSignatureStackEntrySignature(
      uint64_t offset_in_stream,
      uint64_t signature_stack_entries_left,
      mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry,
      const absl::optional<std::vector<uint8_t>>& signature);

  void RunSuccessCallbackAndDestroy(const uint64_t offset_in_stream);

  void RunErrorCallbackAndDestroy(
      const std::string& message,
      mojom::BundleParseErrorType error_type =
          mojom::BundleParseErrorType::kFormatError);

  // Implements SharedBundleDataSource::Observer.
  void OnDisconnect() override;

  scoped_refptr<WebBundleParser::SharedBundleDataSource> data_source_;
  WebBundleParser::ParseIntegrityBlockCallback callback_;

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      signature_stack_;

  base::WeakPtrFactory<IntegrityBlockParser> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_INTEGRITY_BLOCK_PARSER_H_
