// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/files/local_file_provider.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"

namespace app_list {

LocalFileProvider::LocalFileProvider(Profile* profile) : profile_(profile) {
  DCHECK(profile_);
}

LocalFileProvider::~LocalFileProvider() = default;

void LocalFileProvider::Start(const std::u16string& query) {
  // TODO(crbug.com/1154513): Search for local files.
}

ash::AppListSearchResultType LocalFileProvider::ResultType() {
  return ash::AppListSearchResultType::kLocalFile;
}

}  // namespace app_list
