// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_UTIL_H_
#define CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_UTIL_H_

#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"

// A format string for generating paths to COM class in-proc server keys under
// HKEY_CLASSES_ROOT.
extern const wchar_t kClassIdRegistryKeyFormat[];

// Information about the certificate of a file.
struct CertificateInfo {
  // The type of certificate found for the module.
  enum class Type {
    // The module is not signed.
    NO_CERTIFICATE,
    // The module is signed and the certificate is in the module.
    CERTIFICATE_IN_FILE,
    // The module is signed and the certificate is in an external catalog.
    CERTIFICATE_IN_CATALOG,
  };

  CertificateInfo();

  // The type of signature encountered.
  Type type;

  // Path to the file containing the certificate. Empty if |type| is
  // NO_CERTIFICATE.
  base::FilePath path;

  // The "Subject" name of the certificate. This is the signer (e.g.,
  // "Google LLC" or "Microsoft Corporation").
  base::string16 subject;
};

// Extracts information about the certificate of the given |file|, populating
// |certificate_info|. It is expected that |certificate_info| be freshly
// constructed.
void GetCertificateInfo(const base::FilePath& file,
                        CertificateInfo* certificate_info);

// Returns true if the signer name begins with "Microsoft ". Signatures are
// typically "Microsoft Corporation" or "Microsoft Windows", but others may
// exist.
// Note: This is not a secure check to validate the owner of a certificate. It
//       simply does string comparison on the subject name.
bool IsMicrosoftModule(base::StringPiece16 subject);

// Returns a mapping of the value of an environment variable to its name.
// Removes any existing trailing backslash in the values.
//
// e.g. c:\windows\system32 -> %systemroot%
using StringMapping = std::vector<std::pair<base::string16, base::string16>>;
StringMapping GetEnvironmentVariablesMapping(
    const std::vector<base::string16>& environment_variables);

// If |prefix_mapping| contains a matching prefix with |path|, substitutes that
// prefix with its associated value. If multiple matches are found, the longest
// prefix is chosen. Unmodified if no matches are found.
//
// This function expects |prefix_mapping| and |path| to contain lowercase
// strings. Also, |prefix_mapping| must not contain any trailing backslashes.
void CollapseMatchingPrefixInPath(const StringMapping& prefix_mapping,
                                  base::string16* path);

// Reads the file on disk to find out the SizeOfImage and TimeDateStamp
// properties of the module. Returns false on error.
bool GetModuleImageSizeAndTimeDateStamp(const base::FilePath& path,
                                        uint32_t* size_of_image,
                                        uint32_t* time_date_stamp);

namespace internal {

// Removes trailing null characters from the certificate's subject.
// Exposed for testing.
void NormalizeCertificateSubject(base::string16* subject);

}  // namespace internal

#endif  // CHROME_BROWSER_WIN_CONFLICTS_MODULE_INFO_UTIL_H_
