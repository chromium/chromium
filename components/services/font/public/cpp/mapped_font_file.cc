// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/font/public/cpp/mapped_font_file.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"

namespace font_service {
namespace internal {

MappedFontFile::MappedFontFile(uint32_t font_id)
    : font_id_(font_id), observer_(nullptr) {}

bool MappedFontFile::Initialize(base::File file) {
  base::ScopedAllowBlocking allow_mmap;
  return mapped_font_file_.Initialize(std::move(file));
}

SkMemoryStream* MappedFontFile::CreateMemoryStream() {
  DCHECK(mapped_font_file_.IsValid());
  sk_sp<SkData> data =
      SkData::MakeWithProc(mapped_font_file_.data(), mapped_font_file_.length(),
                           &MappedFontFile::ReleaseProc, this);
  if (!data)
    return nullptr;
  AddRef();
  return new SkMemoryStream(std::move(data));
}

MappedFontFile::~MappedFontFile() {
  if (observer_)
    observer_->OnMappedFontFileDestroyed(this);
}

// static
void MappedFontFile::ReleaseProc(const void* ptr, void* context) {
  base::ScopedAllowBlocking allow_munmap;
  static_cast<MappedFontFile*>(context)->Release();
}

}  // namespace internal
}  // namespace font_service
