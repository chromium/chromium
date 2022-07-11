// Copyright 2022 The Chromium Authors. All rights reserved.
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
