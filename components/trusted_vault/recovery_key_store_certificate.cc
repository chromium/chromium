// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_certificate.h"

#include <string>
#include <vector>

#include "base/notimplemented.h"
#include "third_party/libxml/chromium/xml_reader.h"

namespace trusted_vault {

namespace {

// Returns a list of certificates from a certificate tag, traversing its entire
// contents. Ignores anything that is not a <cert> tag directly below the
// certificate tag.
// |xml_reader| must be pointing at the opening element that wraps certificates.
// After processing, |xml_reader| will be pointing at the matching closing tag.
// <intermediates>  <-- Point xml_reader here to start.
//   <cert>cert1</cert>
//   <not-cert>not a certificate</not-cert>  <-- ignored
//   <cert>cert2</cert>
// </intermediates>  <-- xml_reader will point here after processing.
// In this case, the return value would be { cert1, cert2 }.
std::vector<std::string> ParseCertList(XmlReader* xml_reader) {
  std::vector<std::string> cert_list;
  const int starting_depth = xml_reader->Depth();
  while (xml_reader->Read() && xml_reader->Depth() > starting_depth) {
    // Look for a <cert> element nested just under the starting tag.
    if (!xml_reader->IsElement() || xml_reader->Depth() != starting_depth + 1 ||
        xml_reader->NodeName() != "cert") {
      continue;
    }
    std::string cert;
    if (!xml_reader->ReadElementContent(&cert) || cert.empty()) {
      continue;
    }
    cert_list.push_back(std::move(cert));
  }
  return cert_list;
}

}  // namespace

namespace internal {

ParsedRecoveryKeyStoreSigXML::ParsedRecoveryKeyStoreSigXML(
    std::vector<std::string> intermediates,
    std::string certificate,
    std::string signature)
    : intermediates(std::move(intermediates)),
      certificate(std::move(certificate)),
      signature(std::move(signature)) {}

ParsedRecoveryKeyStoreSigXML::ParsedRecoveryKeyStoreSigXML(
    ParsedRecoveryKeyStoreSigXML&&) = default;
ParsedRecoveryKeyStoreSigXML& ParsedRecoveryKeyStoreSigXML::operator=(
    ParsedRecoveryKeyStoreSigXML&&) = default;

ParsedRecoveryKeyStoreSigXML::~ParsedRecoveryKeyStoreSigXML() = default;

std::optional<std::vector<std::string>> ParseRecoveryKeyStoreCertXML(
    std::string_view cert_xml) {
  XmlReader xml_reader;
  xml_reader.Load(cert_xml);
  if (!xml_reader.Read() || xml_reader.NodeName() != "certificate") {
    return std::nullopt;
  }
  while (xml_reader.Read()) {
    // We expect a single <endpoints> tag on the XML. Thus, find the first
    // <endpoints> tag nested under the root and stop processing.
    if (xml_reader.IsElement() && xml_reader.Depth() == 1 &&
        xml_reader.NodeName() == "endpoints") {
      return ParseCertList(&xml_reader);
    }
  }
  return {};
}

std::optional<ParsedRecoveryKeyStoreSigXML> ParseRecoveryKeyStoreSigXML(
    std::string_view sig_xml) {
  XmlReader xml_reader;
  xml_reader.Load(sig_xml);
  if (!xml_reader.Read() || xml_reader.NodeName() != "signature") {
    return std::nullopt;
  }
  std::vector<std::string> intermediates;
  std::string certificate;
  std::string signature;
  while (xml_reader.Read()) {
    // Skip anything that is not an element nested under the root.
    if (!xml_reader.IsElement() || xml_reader.Depth() != 1) {
      continue;
    }
    // We expect a single <intermediates>, <certificate>, and <value> tags. This
    // code only has the last tags take effect, but that should be okay.
    if (xml_reader.NodeName() == "intermediates") {
      intermediates = ParseCertList(&xml_reader);
    } else if (xml_reader.NodeName() == "certificate") {
      if (!xml_reader.ReadElementContent(&certificate)) {
        return std::nullopt;
      }
    } else if (xml_reader.NodeName() == "value") {
      if (!xml_reader.ReadElementContent(&signature)) {
        return std::nullopt;
      }
    }
  }
  if (intermediates.empty() || certificate.empty() || signature.empty()) {
    return std::nullopt;
  }
  return ParsedRecoveryKeyStoreSigXML(
      std::move(intermediates), std::move(certificate), std::move(signature));
}

}  // namespace internal

// static
std::optional<RecoveryKeyStoreCertificate> RecoveryKeyStoreCertificate::Parse(
    std::string_view cert_xml,
    std::string_view sig_xml) {
  NOTIMPLEMENTED();
  return RecoveryKeyStoreCertificate();
}

RecoveryKeyStoreCertificate::RecoveryKeyStoreCertificate() {
  NOTIMPLEMENTED();
}

}  // namespace trusted_vault
