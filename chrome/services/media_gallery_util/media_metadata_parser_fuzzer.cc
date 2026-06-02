// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/media_gallery_util/media_metadata_parser.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "media/filters/memory_data_source.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/test/test_support_android.h"
#endif

class Environment {
 public:
  Environment() {
#if BUILDFLAG(IS_ANDROID)
    // On Android, BrowserTaskEnvironment creates a UI message pump that does
    // not support RunLoop::Run(). This installs a stub pump to allow it in
    // tests.
    base::InitAndroidTestMessageLoop();
#endif
    task_environment_ = std::make_unique<content::BrowserTaskEnvironment>();
  }

 private:
  base::AtExitManager manager_;
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
};

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static Environment env;
  FuzzedDataProvider provider(data.data(), data.size());

  MediaMetadataParser parser(std::make_unique<media::MemoryDataSource>(data),
                             /*mime_type=*/"video/webm",
                             /*get_attached_images=*/provider.ConsumeBool());
  parser.Start(base::DoNothing());
  return 0;
}
