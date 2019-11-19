// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_QUARANTINE_QUARANTINE_H_
#define COMPONENTS_DOWNLOAD_QUARANTINE_QUARANTINE_H_

#include <string>

#include "components/services/quarantine/quarantine.h"

class GURL;

namespace base {
class FilePath;
}

namespace download {

using quarantine::QuarantineFileResult;

// Quarantine a file that was downloaded from the internet.
// See components/services/quarantine/quarantine.h for a full explanation.
//
// Forwards to the definition in components/services/quarantine.
// This is a temporary state before the quarantine service is fully implemented,
// when the call sites will be updated to use the service directly.
//
// TODO: Delete this file (and all of components/download/quarantine)
// when the quarantine service is implemented.
QuarantineFileResult QuarantineFile(const base::FilePath& file,
                                    const GURL& source_url,
                                    const GURL& referrer_url,
                                    const std::string& client_guid);

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_QUARANTINE_QUARANTINE_H_
