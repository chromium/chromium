// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/functional/callback_helpers.h"
#include "chrome/services/media_gallery_util/media_metadata_parser.h"
#include "content/public/test/browser_task_environment.h"
#include "media/filters/memory_data_source.h"

struct Environment {
 public:
  Environment() = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider provider(data, size);

  MediaMetadataParser parser(
      std::make_unique<media::MemoryDataSource>(data, size),
      /*mime_type=*/"video/webm",
      /*get_attached_images=*/provider.ConsumeBool());
  parser.Start(base::DoNothing());
  return 0;
}
