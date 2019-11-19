// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CDM_MANIFEST_H_
#define CHROME_COMMON_MEDIA_CDM_MANIFEST_H_

namespace base {
class FilePath;
class Value;
class Version;
}

namespace content {
struct CdmCapability;
}

// Returns whether the CDM's API versions, as specified in the manifest, are
// supported in this Chrome binary and not disabled at run time.
// Checks the module API, CDM interface API, and Host API.
// This should never fail except in rare cases where the component has not been
// updated recently or the user downgrades Chrome.
bool IsCdmManifestCompatibleWithChrome(const base::Value& manifest);

// Extracts the necessary information from |manifest| and updates |capability|.
// Returns true on success, false if there are errors in the manifest.
// If this method returns false, |capability| may or may not be updated.
bool ParseCdmManifest(const base::Value& manifest,
                      content::CdmCapability* capability);

// Reads the file |manifest_path| which is assumed to be a CDM manifest and
// extracts the necessary information from it to update |version| and
// |capability|. This also verifies that the read CDM manifest is compatible
// with Chrome (by calling IsCdmManifestCompatibleWithChrome()). Returns true on
// success, false if there are errors in the file or the manifest is not
// compatible with this version of Chrome. If this method returns false,
// |version| and |capability| may or may not be updated.
bool ParseCdmManifestFromPath(const base::FilePath& manifest_path,
                              base::Version* version,
                              content::CdmCapability* capability);

#endif  // CHROME_COMMON_MEDIA_CDM_MANIFEST_H_
