// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_
#define COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/attribute_map_parser.h"
#include "components/web_package/signed_web_bundles/types.h"

namespace web_package {

// This class is responsible for parsing a single signature entry from the
// signature stack of the integrity block of a signed web bundle.
class SignatureStackEntryParser {
 public:
  // In case of success the callback returns the signature stack entry and the
  // offset in the stream corresponding to the end of the entry.
  using SignatureEntryParsedCallback = base::OnceCallback<void(
      base::expected<
          std::pair<mojom::BundleIntegrityBlockSignatureStackEntryPtr,
                    uint64_t>,
          std::string>)>;

  explicit SignatureStackEntryParser(mojom::BundleDataSource& data_source,
                                     SignatureEntryParsedCallback callback);

  ~SignatureStackEntryParser();

  void Parse(uint64_t offset);

 private:
  void ReadSignatureStructure(const std::optional<BinaryData>& data);
  void GetAttributesMap(AttributeMapParser::ParsingResult result);
  void ReadAttributesMapBytes(uint64_t num_bytes,
                              const std::optional<BinaryData>& data);
  void ReadSignatureHeader(const std::optional<BinaryData>& data);
  void ReadSignatureValue(const std::optional<BinaryData>& data);
  void EvaluateSignatureEntry(BinaryData data);
  void RunErrorCallback(std::string message);

  mojom::BundleIntegrityBlockSignatureStackEntryPtr signature_stack_entry_;
  AttributesMap attributes_map_;
  std::unique_ptr<AttributeMapParser> attribute_map_parser_;

  uint64_t offset_in_stream_;
  const raw_ref<mojom::BundleDataSource> data_source_;

  SignatureEntryParsedCallback callback_;
  base::WeakPtrFactory<SignatureStackEntryParser> weak_factory_{this};
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SIGNED_WEB_BUNDLES_SIGNATURE_ENTRY_PARSER_H_
