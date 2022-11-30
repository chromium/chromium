// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_PUBLIC_MOJOM_SAFE_DOCUMENT_ANALYZER_MOJOM_TRAITS_H_
#define CHROME_SERVICES_FILE_UTIL_PUBLIC_MOJOM_SAFE_DOCUMENT_ANALYZER_MOJOM_TRAITS_H_

#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "chrome/common/safe_browsing/document_analyzer_results.h"
#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

#if !BUILDFLAG(FULL_SAFE_BROWSING) || \
    (!BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_WIN))
#error BUILDFLAG(FULL_SAFE_BROWSING) should be set and either OS_LINUX or OS_WIN defined.
#endif

namespace mojo {

using DocumentProcessingInfo =
    safe_browsing::ClientDownloadRequest::DocumentProcessingInfo;
using MojomMaldocaErrorType = chrome::mojom::MaldocaErrorType;

template <>
struct EnumTraits<chrome::mojom::MaldocaErrorType,
                  safe_browsing::ClientDownloadRequest::DocumentProcessingInfo::
                      MaldocaErrorType> {
  static chrome::mojom::MaldocaErrorType ToMojom(
      safe_browsing::ClientDownloadRequest::DocumentProcessingInfo::
          MaldocaErrorType input);
  static bool FromMojom(chrome::mojom::MaldocaErrorType input,
                        safe_browsing::ClientDownloadRequest::
                            DocumentProcessingInfo::MaldocaErrorType* output);
};

template <>
struct StructTraits<chrome::mojom::SafeDocumentAnalyzerResultsDataView,
                    safe_browsing::DocumentAnalyzerResults> {
  static bool success(const safe_browsing::DocumentAnalyzerResults& results) {
    return results.success;
  }
  static MojomMaldocaErrorType error_code(
      const safe_browsing::DocumentAnalyzerResults& results) {
    switch (results.error_code) {
      case DocumentProcessingInfo::OK:
        return MojomMaldocaErrorType::kOk;
      case DocumentProcessingInfo::CANCELLED:
        return MojomMaldocaErrorType::kCancelled;
      case DocumentProcessingInfo::UNKNOWN:
        return MojomMaldocaErrorType::kUnknown;
      case DocumentProcessingInfo::INVALID_ARGUMENT:
        return MojomMaldocaErrorType::kInvalidArgument;
      case DocumentProcessingInfo::DEADLINE_EXCEEDED:
        return MojomMaldocaErrorType::kDeadlineExceeded;
      case DocumentProcessingInfo::NOT_FOUND:
        return MojomMaldocaErrorType::kNotFound;
      case DocumentProcessingInfo::ALREADY_EXISTS:
        return MojomMaldocaErrorType::kAlreadyExists;
      case DocumentProcessingInfo::PERMISSION_DENIED:
        return MojomMaldocaErrorType::kPermissionDenied;
      case DocumentProcessingInfo::RESOURCE_EXHAUSTED:
        return MojomMaldocaErrorType::kResourceExhausted;
      case DocumentProcessingInfo::FAILED_PRECONDITION:
        return MojomMaldocaErrorType::kFailedPrecondition;
      case DocumentProcessingInfo::ABORTED:
        return MojomMaldocaErrorType::kAborted;
      case DocumentProcessingInfo::OUT_OF_RANGE:
        return MojomMaldocaErrorType::kOutOfRange;
      case DocumentProcessingInfo::UNIMPLEMENTED:
        return MojomMaldocaErrorType::kUnimplemented;
      case DocumentProcessingInfo::INTERNAL:
        return MojomMaldocaErrorType::kInternal;
      case DocumentProcessingInfo::UNAVAILABLE:
        return MojomMaldocaErrorType::kUnavailable;
      case DocumentProcessingInfo::DATA_LOSS:
        return MojomMaldocaErrorType::kDataLoss;
      case DocumentProcessingInfo::UNAUTHENTICATED:
        return MojomMaldocaErrorType::kUnauthenticated;
      case DocumentProcessingInfo::DOC_TYPE_INFERENCE_FAILED:
        return MojomMaldocaErrorType::kDocTypeInferenceFailed;
      case DocumentProcessingInfo::UNSUPPORTED_DOC_TYPE:
        return MojomMaldocaErrorType::kUnsupportedDocType;
      case DocumentProcessingInfo::SANDBOX_ERROR:
        NOTREACHED();
        return MojomMaldocaErrorType::kSandboxError;
      case DocumentProcessingInfo::ARCHIVE_CORRUPTED:
        return MojomMaldocaErrorType::kArchiveCorrupted;
      case DocumentProcessingInfo::OLE_DIR_PARSING_FAILED:
        return MojomMaldocaErrorType::kOLEDirParsingFailed;
      case DocumentProcessingInfo::OLE_FAT_HEADER_PARSING_FAILED:
        return MojomMaldocaErrorType::kOLEFatHeaderParsingFailed;
      case DocumentProcessingInfo::PREFIXED_ANSI_STRING_HEADER_TOO_SHORT:
        return MojomMaldocaErrorType::kPrefixedANSIStringHeaderTooShort;
      case DocumentProcessingInfo::PREFIXED_ANSI_STRING_CONTENT_TOO_SHORT:
        return MojomMaldocaErrorType::kPrefixedANSIStringContentTooShort;
      case DocumentProcessingInfo::CLIPBOARD_FORMAT_OR_ANSI_STRING_TOO_SHORT:
        return MojomMaldocaErrorType::kClipboardFormatOrANSIStringTooShort;
      case DocumentProcessingInfo::BOF_HEADER_TOO_SHORT:
        return MojomMaldocaErrorType::kBOFHeaderTooShort;
      case DocumentProcessingInfo::NOT_BIFF_FORMAT:
        return MojomMaldocaErrorType::kNotBIFFFromat;
      case DocumentProcessingInfo::FAIL_PARSE_BIFF_VERSION:
        return MojomMaldocaErrorType::kFailParseBIFFVersion;
      case DocumentProcessingInfo::INVALID_DDE_OLE_LINK:
        return MojomMaldocaErrorType::kInvalidDDEOLELink;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_SIZE_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseSizeFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_SIZE_MISMATCH:
        return MojomMaldocaErrorType::kOLENativeEmbeddedSizeMismatch;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_TYPE_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseTypeFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILENAME_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseFilenameFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILEPATH_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseFilepathFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_RESERVED_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseReservedFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_TEMPPATH_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseTemppathFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILESIZE_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseFilesizeFail;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_FILESIZE_MISMATCH:
        return MojomMaldocaErrorType::kOLENativeEmbeddedFilesizeMismatch;
      case DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_CONTENT_FAIL:
        return MojomMaldocaErrorType::kOLENativeEmbeddedParseContentFail;
      case DocumentProcessingInfo::INVALID_OLE2_HEADER:
        return MojomMaldocaErrorType::kInvalidOLE2Header;
      case DocumentProcessingInfo::INVALID_FAT_HEADER:
        return MojomMaldocaErrorType::kInvalidFatHeader;
      case DocumentProcessingInfo::EMPTY_FAT_HEADER:
        return MojomMaldocaErrorType::kEmptyFatHeader;
      case DocumentProcessingInfo::INVALID_ROOT_DIR:
        return MojomMaldocaErrorType::kInvalidRootDir;
      case DocumentProcessingInfo::MISSING_FILE_IN_ARCHIVE:
        return MojomMaldocaErrorType::kMissingFileInArchive;
      case DocumentProcessingInfo::INVALID_XML_DOC:
        return MojomMaldocaErrorType::kInvalidXMLDoc;
      case DocumentProcessingInfo::MISSING_PROPERTIES:
        return MojomMaldocaErrorType::kMissingProperties;
      case DocumentProcessingInfo::NOT_IMPLEMENTED_FOR_CHROME:
        return MojomMaldocaErrorType::kNotImplementedForChrome;
      case DocumentProcessingInfo::NOT_IMPLEMENTED:
        return MojomMaldocaErrorType::kNotImplemented;
      case DocumentProcessingInfo::MISSING_ENCODING:
        return MojomMaldocaErrorType::kMissingEncoding;
      default:
        return MojomMaldocaErrorType::kUnknown;
    }
  }

  static bool has_macros(
      const safe_browsing::DocumentAnalyzerResults& results) {
    return results.has_macros;
  }

  static base::StringPiece error_message(
      const safe_browsing::DocumentAnalyzerResults& results) {
    if (results.error_message.empty()) {
      return base::StringPiece();
    }
    return results.error_message;
  }

  static bool Read(chrome::mojom::SafeDocumentAnalyzerResultsDataView data,
                   safe_browsing::DocumentAnalyzerResults* out_results);
};

}  // namespace mojo

#endif  // CHROME_SERVICES_FILE_UTIL_PUBLIC_MOJOM_SAFE_DOCUMENT_ANALYZER_MOJOM_TRAITS_H_
