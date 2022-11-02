// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <tuple>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/url_formatter/url_fixer.h"
#include "url/third_party/mozilla/url_parse.h"

namespace {

// Performs initialization and holds state that's shared across all runs.
class Environment {
 public:
  Environment() {
    CHECK(base::i18n::InitializeICU());
    logging::SetMinLogLevel(logging::LOG_FATAL);
  }

 private:
  base::AtExitManager at_exit_manager_;
};

base::FilePath GenerateFuzzedFilePath(FuzzedDataProvider& provider) {
  const std::string raw_string = provider.ConsumeRandomLengthString();
#if BUILDFLAG(IS_WIN)
  return base::FilePath(base::UTF8ToWide(raw_string));
#else
  return base::FilePath(raw_string);
#endif
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;
  FuzzedDataProvider provider(data, size);

  switch (provider.ConsumeIntegralInRange<int>(0, 3)) {
    case 0: {
      std::string text = provider.ConsumeRandomLengthString();
      url::Parsed parts;
      std::ignore = url_formatter::SegmentURL(text, &parts);
      break;
    }
    case 1: {
      std::u16string text =
          base::UTF8ToUTF16(provider.ConsumeRandomLengthString());
      url::Parsed parts;
      std::ignore = url_formatter::SegmentURL(text, &parts);
      break;
    }
    case 2: {
      std::ignore =
          url_formatter::FixupURL(provider.ConsumeRandomLengthString(),
                                  provider.ConsumeRandomLengthString());
      break;
    }
    case 3: {
      std::ignore = url_formatter::FixupRelativeFile(
          GenerateFuzzedFilePath(provider), GenerateFuzzedFilePath(provider));
      break;
    }
  }

  return 0;
}
