// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_H_
#define COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_H_

#include <string>

#include "build/build_config.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"

class GURL;

namespace base {
class FilePath;
}

namespace quarantine {

using mojom::QuarantineFileResult;

// Quarantine a file that was downloaded from the internet.
//
// Ensures that |file| is handled as safely as possible given that it was
// downloaded from |source_url|. The details of how a downloaded file is handled
// are platform dependent. Please refer to the individual quarantine_<os>
// implementation.
//
// This function should be called for all files downloaded from the internet and
// placed in a manner discoverable by the user, or exposed to an external
// application. Furthermore, it should be called:
//
// * **AFTER** all the data has been written to the file. On Windows, registered
//   anti-virus products will be invoked for scanning the contents of the file.
//   Hence it's important to have the final contents of the file be available at
//   the point at which this function is called.
//
//   Exception: Zero-length files will be handled solely on the basis of the
//   |source_url| and the file type. This exception accommodates situations
//   where the file contents cannot be determined before it is made visible to
//   an external application.
//
// * **AFTER** the file has been renamed to its final name. The file type is
//   significant and is derived from the filename.
//
// * **BEFORE** the file is made visible to an external application or the user.
//   Security checks and mark-of-the-web annotations must be made prior to
//   exposing the file externally.
//
// Note that it is possible for this method to take a long time to complete
// (several seconds or more). In addition to blocking during this time, this
// delay also introduces a window during which a browser shutdown may leave the
// downloaded file unannotated.
//
// Parameters:
//   |file| : Final name of the file.
//   |source_url|: URL from which the file content was downloaded. This is empty
//     for off-the-record download.
//   |referrer_url|: Referring URL. This is empty for off-the-record download.
//   `request_initiator`: Origin initiating the request. This is meant to
//     replace the source URL when the source URL is not suitable for use in an
//     annotation (e.g. a data URL).
//   |client_guid|: Only used on Windows. Identifies the client application
//     that downloaded the file.
//   |callback|: Will be called with the quarantine result on completion.
//
// Note: The |source_url| and |referrer_url| will be stripped of unnecessary
//   parts using SanitizeUrlForQuarantine() before they are used for annotation
//   or notification purposes. If the URLs are sensitive -- e.g. because the
//   download was made using an off-the-record profile -- then pass in an empty
//   GURL() instead.
void QuarantineFile(const base::FilePath& file,
                    const GURL& source_url,
                    const GURL& referrer_url,
                    const std::optional<url::Origin>& request_initiator,
                    const std::string& client_guid,
                    mojom::Quarantine::QuarantineFileCallback callback);

#if BUILDFLAG(IS_WIN)
QuarantineFileResult SetInternetZoneIdentifierDirectly(
    const base::FilePath& full_path,
    const GURL& source_url,
    const GURL& referrer_url);
#endif

}  // namespace quarantine

#endif  // COMPONENTS_SERVICES_QUARANTINE_QUARANTINE_H_
