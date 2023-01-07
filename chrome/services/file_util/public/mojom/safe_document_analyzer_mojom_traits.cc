// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/public/mojom/safe_document_analyzer_mojom_traits.h"
#include "base/notreached.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"

namespace mojo {

MojomMaldocaErrorType
EnumTraits<MojomMaldocaErrorType, DocumentProcessingInfo::MaldocaErrorType>::
    ToMojom(DocumentProcessingInfo::MaldocaErrorType input) {
  switch (input) {
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

bool EnumTraits<MojomMaldocaErrorType,
                DocumentProcessingInfo::MaldocaErrorType>::
    FromMojom(MojomMaldocaErrorType input,
              DocumentProcessingInfo::MaldocaErrorType* output) {
  switch (input) {
    case MojomMaldocaErrorType::kOk:
      *output = DocumentProcessingInfo::OK;
      return true;
    case MojomMaldocaErrorType::kCancelled:
      *output = DocumentProcessingInfo::CANCELLED;
      return true;
    case MojomMaldocaErrorType::kUnknown:
      *output = DocumentProcessingInfo::UNKNOWN;
      return true;
    case MojomMaldocaErrorType::kInvalidArgument:
      *output = DocumentProcessingInfo::INVALID_ARGUMENT;
      return true;
    case MojomMaldocaErrorType::kDeadlineExceeded:
      *output = DocumentProcessingInfo::DEADLINE_EXCEEDED;
      return true;
    case MojomMaldocaErrorType::kNotFound:
      *output = DocumentProcessingInfo::NOT_FOUND;
      return true;
    case MojomMaldocaErrorType::kAlreadyExists:
      *output = DocumentProcessingInfo::ALREADY_EXISTS;
      return true;
    case MojomMaldocaErrorType::kPermissionDenied:
      *output = DocumentProcessingInfo::PERMISSION_DENIED;
      return true;
    case MojomMaldocaErrorType::kResourceExhausted:
      *output = DocumentProcessingInfo::RESOURCE_EXHAUSTED;
      return true;
    case MojomMaldocaErrorType::kFailedPrecondition:
      *output = DocumentProcessingInfo::FAILED_PRECONDITION;
      return true;
    case MojomMaldocaErrorType::kAborted:
      *output = DocumentProcessingInfo::ABORTED;
      return true;
    case MojomMaldocaErrorType::kOutOfRange:
      *output = DocumentProcessingInfo::OUT_OF_RANGE;
      return true;
    case MojomMaldocaErrorType::kUnimplemented:
      *output = DocumentProcessingInfo::UNIMPLEMENTED;
      return true;
    case MojomMaldocaErrorType::kInternal:
      *output = DocumentProcessingInfo::INTERNAL;
      return true;
    case MojomMaldocaErrorType::kUnavailable:
      *output = DocumentProcessingInfo::UNAVAILABLE;
      return true;
    case MojomMaldocaErrorType::kDataLoss:
      *output = DocumentProcessingInfo::DATA_LOSS;
      return true;
    case MojomMaldocaErrorType::kUnauthenticated:
      *output = DocumentProcessingInfo::UNAUTHENTICATED;
      return true;
    case MojomMaldocaErrorType::kDocTypeInferenceFailed:
      *output = DocumentProcessingInfo::DOC_TYPE_INFERENCE_FAILED;
      return true;
    case MojomMaldocaErrorType::kUnsupportedDocType:
      *output = DocumentProcessingInfo::UNSUPPORTED_DOC_TYPE;
      return true;
    case MojomMaldocaErrorType::kSandboxError:
      NOTREACHED();
      *output = DocumentProcessingInfo::SANDBOX_ERROR;
      return true;
    case MojomMaldocaErrorType::kArchiveCorrupted:
      *output = DocumentProcessingInfo::ARCHIVE_CORRUPTED;
      return true;
    case MojomMaldocaErrorType::kOLEDirParsingFailed:
      *output = DocumentProcessingInfo::OLE_DIR_PARSING_FAILED;
      return true;
    case MojomMaldocaErrorType::kOLEFatHeaderParsingFailed:
      *output = DocumentProcessingInfo::OLE_FAT_HEADER_PARSING_FAILED;
      return true;
    case MojomMaldocaErrorType::kPrefixedANSIStringHeaderTooShort:
      *output = DocumentProcessingInfo::PREFIXED_ANSI_STRING_HEADER_TOO_SHORT;
      return true;
    case MojomMaldocaErrorType::kPrefixedANSIStringContentTooShort:
      *output = DocumentProcessingInfo::PREFIXED_ANSI_STRING_CONTENT_TOO_SHORT;
      return true;
    case MojomMaldocaErrorType::kClipboardFormatOrANSIStringTooShort:
      *output =
          DocumentProcessingInfo::CLIPBOARD_FORMAT_OR_ANSI_STRING_TOO_SHORT;
      return true;
    case MojomMaldocaErrorType::kBOFHeaderTooShort:
      *output = DocumentProcessingInfo::BOF_HEADER_TOO_SHORT;
      return true;
    case MojomMaldocaErrorType::kNotBIFFFromat:
      *output = DocumentProcessingInfo::NOT_BIFF_FORMAT;
      return true;
    case MojomMaldocaErrorType::kFailParseBIFFVersion:
      *output = DocumentProcessingInfo::FAIL_PARSE_BIFF_VERSION;
      return true;
    case MojomMaldocaErrorType::kInvalidDDEOLELink:
      *output = DocumentProcessingInfo::INVALID_DDE_OLE_LINK;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseSizeFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_SIZE_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedSizeMismatch:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_SIZE_MISMATCH;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseTypeFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_TYPE_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseFilenameFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILENAME_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseFilepathFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILEPATH_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseReservedFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_RESERVED_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseTemppathFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_TEMPPATH_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseFilesizeFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_FILESIZE_FAIL;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedFilesizeMismatch:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_FILESIZE_MISMATCH;
      return true;
    case MojomMaldocaErrorType::kOLENativeEmbeddedParseContentFail:
      *output = DocumentProcessingInfo::OLE_NATIVE_EMBEDDED_PARSE_CONTENT_FAIL;
      return true;
    case MojomMaldocaErrorType::kInvalidOLE2Header:
      *output = DocumentProcessingInfo::INVALID_OLE2_HEADER;
      return true;
    case MojomMaldocaErrorType::kInvalidFatHeader:
      *output = DocumentProcessingInfo::INVALID_FAT_HEADER;
      return true;
    case MojomMaldocaErrorType::kEmptyFatHeader:
      *output = DocumentProcessingInfo::EMPTY_FAT_HEADER;
      return true;
    case MojomMaldocaErrorType::kInvalidRootDir:
      *output = DocumentProcessingInfo::INVALID_ROOT_DIR;
      return true;
    case MojomMaldocaErrorType::kMissingFileInArchive:
      *output = DocumentProcessingInfo::MISSING_FILE_IN_ARCHIVE;
      return true;
    case MojomMaldocaErrorType::kInvalidXMLDoc:
      *output = DocumentProcessingInfo::INVALID_XML_DOC;
      return true;
    case MojomMaldocaErrorType::kMissingProperties:
      *output = DocumentProcessingInfo::MISSING_PROPERTIES;
      return true;
    case MojomMaldocaErrorType::kNotImplementedForChrome:
      *output = DocumentProcessingInfo::NOT_IMPLEMENTED_FOR_CHROME;
      return true;
    case MojomMaldocaErrorType::kNotImplemented:
      *output = DocumentProcessingInfo::NOT_IMPLEMENTED;
      return true;
    case MojomMaldocaErrorType::kMissingEncoding:
      *output = DocumentProcessingInfo::MISSING_ENCODING;
      return true;
  }
  NOTREACHED();
  return false;
}

bool StructTraits<chrome::mojom::SafeDocumentAnalyzerResultsDataView,
                  safe_browsing::DocumentAnalyzerResults>::
    Read(chrome::mojom::SafeDocumentAnalyzerResultsDataView data,
         safe_browsing::DocumentAnalyzerResults* out_results) {
  base::StringPiece error_message;
  MojomMaldocaErrorType error_code;
  if (!data.ReadErrorCode(&error_code))
    return false;
  EnumTraits<MojomMaldocaErrorType, DocumentProcessingInfo::MaldocaErrorType>::
      FromMojom(error_code, &out_results->error_code);

  if (!data.ReadErrorMessage(&error_message))
    return false;
  out_results->error_message = std::string(error_message);

  out_results->success = data.success();

  out_results->has_macros = data.has_macros();
  return true;
}

}  // namespace mojo
