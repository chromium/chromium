// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/binary_feature_extractor.h"

#include <windows.h>
#include <softpub.h>
#include <stddef.h>
#include <stdint.h>
#include <wintrust.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/threading/scoped_thread_priority.h"
#include "chrome/common/safe_browsing/pe_image_reader_win.h"
#include "components/safe_browsing/proto/csd.pb.h"

namespace safe_browsing {

namespace {

// An EnumCertificatesCallback that collects each SignedData blob.
bool OnCertificateEntry(uint16_t revision,
                        uint16_t certificate_type,
                        const uint8_t* certificate_data,
                        size_t certificate_data_size,
                        void* context) {
  google::protobuf::RepeatedPtrField<std::string>* signed_data =
      reinterpret_cast<google::protobuf::RepeatedPtrField<std::string>*>(
          context);

  if (revision == WIN_CERT_REVISION_2_0 &&
      certificate_type == WIN_CERT_TYPE_PKCS_SIGNED_DATA) {
    signed_data->Add()->assign(certificate_data,
                               certificate_data + certificate_data_size);
  }
  return true;
}

}  // namespace

void BinaryFeatureExtractor::CheckSignature(
    const base::FilePath& file_path,
    ClientDownloadRequest_SignatureInfo* signature_info) {
  // Mitigate the issues caused by loading DLLs on a background thread
  // (http://crbug/973868).
  base::ScopedThreadMayLoadLibraryOnBackgroundThread priority_boost(FROM_HERE);

  DVLOG(2) << "Checking signature for " << file_path.value();

  WINTRUST_FILE_INFO file_info = {0};
  file_info.cbStruct = sizeof(file_info);
  file_info.pcwszFilePath = file_path.value().c_str();
  file_info.hFile = NULL;
  file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data = {0};
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.pFile = &file_info;
  wintrust_data.dwStateAction = WTD_STATEACTION_VERIFY;
  wintrust_data.hWVTStateData = NULL;
  wintrust_data.pwszURLReference = NULL;
  // Disallow revocation checks over the network.
  wintrust_data.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;

  // The WINTRUST_ACTION_GENERIC_VERIFY_V2 policy verifies that the certificate
  // chains up to a trusted root CA, and that it has appropriate permission to
  // sign code.
  GUID policy_guid = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  LONG result = WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                               &policy_guid,
                               &wintrust_data);

  CRYPT_PROVIDER_DATA* prov_data = WTHelperProvDataFromStateData(
      wintrust_data.hWVTStateData);
  if (prov_data) {
    if (prov_data->csSigners > 0) {
      signature_info->set_trusted(result == ERROR_SUCCESS);
    }
    for (DWORD i = 0; i < prov_data->csSigners; ++i) {
      const CERT_CHAIN_CONTEXT* cert_chain_context =
          prov_data->pasSigners[i].pChainContext;
      if (!cert_chain_context)
        break;
      for (DWORD j = 0; j < cert_chain_context->cChain; ++j) {
        CERT_SIMPLE_CHAIN* simple_chain = cert_chain_context->rgpChain[j];
        ClientDownloadRequest_CertificateChain* chain =
            signature_info->add_certificate_chain();
        if (!simple_chain)
          break;
        for (DWORD k = 0; k < simple_chain->cElement; ++k) {
          CERT_CHAIN_ELEMENT* element = simple_chain->rgpElement[k];
          chain->add_element()->set_certificate(
              element->pCertContext->pbCertEncoded,
              element->pCertContext->cbCertEncoded);
        }
      }
    }

    // Free the provider data.
    wintrust_data.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE),
                   &policy_guid, &wintrust_data);
  }
}

bool BinaryFeatureExtractor::ExtractImageFeaturesFromData(
    const uint8_t* data, size_t data_size,
    ExtractHeadersOption options,
    ClientDownloadRequest_ImageHeaders* image_headers,
    google::protobuf::RepeatedPtrField<std::string>* signed_data) {
  PeImageReader pe_image;
  if (!pe_image.Initialize(data, data_size))
    return false;

  // Copy the headers.
  ClientDownloadRequest_PEImageHeaders* pe_headers =
      image_headers->mutable_pe_headers();
  pe_headers->set_dos_header(pe_image.GetDosHeader(), sizeof(IMAGE_DOS_HEADER));
  pe_headers->set_file_header(pe_image.GetCoffFileHeader(),
                              sizeof(IMAGE_FILE_HEADER));
  size_t optional_header_size = 0;
  const uint8_t* optional_header_data =
      pe_image.GetOptionalHeaderData(&optional_header_size);
  if (pe_image.GetWordSize() == PeImageReader::WORD_SIZE_32) {
    pe_headers->set_optional_headers32(optional_header_data,
                                       optional_header_size);
  } else {
    pe_headers->set_optional_headers64(optional_header_data,
                                       optional_header_size);
  }
  const size_t number_of_sections = pe_image.GetNumberOfSections();
  for (size_t i = 0; i != number_of_sections; ++i) {
    pe_headers->add_section_header(pe_image.GetSectionHeaderAt(i),
                                   sizeof(IMAGE_SECTION_HEADER));
  }
  if (!(options & BinaryFeatureExtractor::kOmitExports)) {
    size_t export_size = 0;
    const uint8_t* export_section = pe_image.GetExportSection(&export_size);
    if (export_section)
      pe_headers->set_export_section_data(export_section, export_size);
  }
  size_t number_of_debug_entries = pe_image.GetNumberOfDebugEntries();
  for (size_t i = 0; i != number_of_debug_entries; ++i) {
    const uint8_t* raw_data = NULL;
    size_t raw_data_size = 0;
    const IMAGE_DEBUG_DIRECTORY* directory_entry =
        pe_image.GetDebugEntry(i, &raw_data, &raw_data_size);
    if (directory_entry) {
      ClientDownloadRequest_PEImageHeaders_DebugData* debug_data =
          pe_headers->add_debug_data();
      debug_data->set_directory_entry(directory_entry,
                                      sizeof(*directory_entry));
      if (raw_data)
        debug_data->set_raw_data(raw_data, raw_data_size);
    }
  }

  if (signed_data)
    pe_image.EnumCertificates(&OnCertificateEntry, signed_data);

  return true;
}

}  // namespace safe_browsing
