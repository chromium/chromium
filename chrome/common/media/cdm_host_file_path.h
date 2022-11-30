// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_
#define CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_

#include <vector>

#include "media/cdm/cdm_host_file.h"

// Gets a list of CDM host file paths and put them in |cdm_host_file_paths|.
void AddCdmHostFilePaths(
    std::vector<media::CdmHostFilePath>* cdm_host_file_paths);

#endif  // CHROME_COMMON_MEDIA_CDM_HOST_FILE_PATH_H_
