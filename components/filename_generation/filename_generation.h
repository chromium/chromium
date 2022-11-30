// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILENAME_GENERATION_FILENAME_GENERATION_H_
#define COMPONENTS_FILENAME_GENERATION_FILENAME_GENERATION_H_

#include <string>

#include "base/files/file_path.h"

class GURL;

namespace filename_generation {

// Returns extension for supported MIME types (for example, for "text/plain"
// it returns "txt").
const base::FilePath::CharType* ExtensionForMimeType(
    const std::string& contents_mime_type);

// Ensures that the file name has a proper extension for HTML by adding ".htm"
// if necessary.
base::FilePath EnsureHtmlExtension(const base::FilePath& name);

// Ensures that the file name has a proper extension for supported formats
// if necessary.
base::FilePath EnsureMimeExtension(const base::FilePath& name,
                                   const std::string& contents_mime_type);

// Function for generating a filename based on |title|, if it is empty, |url|
// will be used as a fallback.
base::FilePath GenerateFilename(const std::u16string& title,
                                const GURL& url,
                                bool can_save_as_complete,
                                std::string contents_mime_type);

// Truncates path->BaseName() to make path->BaseName().value().size() <= limit.
// - It keeps the extension as is. Only truncates the body part.
// - Only truncates if the base filename can maintain a minimum length
//   (currently a hardcoded internval constant kTruncatedNameLengthLowerbound,
//   but could be parameterized if ever required).
//   If it was unable to shorten the name, returns false.
bool TruncateFilename(base::FilePath* path, size_t limit);

}  // namespace filename_generation

#endif  // COMPONENTS_FILENAME_GENERATION_FILENAME_GENERATION_H_
