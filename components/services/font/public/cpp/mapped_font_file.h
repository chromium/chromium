// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FONT_PUBLIC_CPP_MAPPED_FONT_FILE_H_
#define COMPONENTS_SERVICES_FONT_PUBLIC_CPP_MAPPED_FONT_FILE_H_

#include <stdint.h>

#include "base/containers/weak_value_table.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "third_party/skia/include/core/SkStream.h"

namespace font_service {
namespace internal {

// Owns the memory of the mmaped file that we get back from the font_service.
//
// This class is an implementation detail and shouldn't be used by consumers.
class MappedFontFile
    : public base::subtle::RefCountedWeakValue<uint32_t, MappedFontFile> {
 public:
  explicit MappedFontFile(uint32_t font_id);

  uint32_t GetKey() const { return font_id_; }
  uint32_t font_id() const { return font_id_; }

  bool Initialize(base::File file);

  SkMemoryStream* CreateMemoryStream();

 private:
  friend class base::subtle::RefCountedWeakValue<uint32_t, MappedFontFile>;

  ~MappedFontFile();

  static void ReleaseProc(const void* ptr, void* context);

  uint32_t font_id_;
  base::MemoryMappedFile mapped_font_file_;
};

}  // namespace internal
}  // namespace font_service

#endif  // COMPONENTS_SERVICES_FONT_PUBLIC_CPP_MAPPED_FONT_FILE_H_
