// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/ranking/mrfu_cache.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/time/time.h"

namespace app_list {
namespace {

constexpr base::TimeDelta kSaveDelay = base::TimeDelta::FromSeconds(30);

}  // namespace

MrfuCache::MrfuCache(const base::FilePath& path)
    : proto_(path, kSaveDelay, base::DoNothing(), base::DoNothing()) {}

MrfuCache::~MrfuCache() {}

void MrfuCache::Use(const std::string& value) {
  // TODO(crbug.com/1199206): Unimplemented.
}

float MrfuCache::Get(const std::string& value) {
  // TODO(crbug.com/1199206): Unimplemented.
  return 0.0f;
}

}  // namespace app_list
