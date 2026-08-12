// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_WOF_COMPRESSION_H_
#define CHROME_INSTALLER_SETUP_WOF_COMPRESSION_H_

namespace base {
class FilePath;
}

class WorkItemList;

namespace installer {

// Returns true if `path` names a file whose data is held by the Windows Overlay
// Filter rather than in the file's own data stream.
bool IsFileWofCompressed(const base::FilePath& path);

// Adds a work item to `list` that compresses the locale packs in `version_dir`
// with Windows Overlay Filter (WOF) LZX.
//
// Chrome installs one locale pack per supported language and a given machine
// reads one of them. They are also the most compressible thing in the payload,
// holding plain UTF-8 strings rather than the already-compressed data that
// makes up most of an install: measured on a 153.0.7978.0 x64 install, they
// shrink from 54,377,999 to 14,556,166 bytes, which is 38.0 MiB off a 561.9 MiB
// install.
//
// WOF compression is transparent. The file keeps its logical size and its
// contents, and the filesystem decompresses it on read, so nothing above the
// storage layer needs to know. That is what makes it safe to apply to files
// that are memory mapped, as these are.
//
// The item is best effort and is not rolled back: leaving a file uncompressed
// is not a failure worth failing an install over.
void AddWofCompressionWorkItems(const base::FilePath& version_dir,
                                WorkItemList* list);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_WOF_COMPRESSION_H_
