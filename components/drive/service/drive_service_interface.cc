// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/drive/service/drive_service_interface.h"

namespace drive {

AddNewDirectoryOptions::AddNewDirectoryOptions()
    : visibility(google_apis::drive::FILE_VISIBILITY_DEFAULT) {
}

AddNewDirectoryOptions::AddNewDirectoryOptions(
    const AddNewDirectoryOptions& other) = default;

AddNewDirectoryOptions::~AddNewDirectoryOptions() = default;

UploadNewFileOptions::UploadNewFileOptions() = default;

UploadNewFileOptions::UploadNewFileOptions(const UploadNewFileOptions& other) =
    default;

UploadNewFileOptions::~UploadNewFileOptions() = default;

UploadExistingFileOptions::UploadExistingFileOptions() = default;

UploadExistingFileOptions::UploadExistingFileOptions(
    const UploadExistingFileOptions& other) = default;

UploadExistingFileOptions::~UploadExistingFileOptions() = default;

}  // namespace drive
