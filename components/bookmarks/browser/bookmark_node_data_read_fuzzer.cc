// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/i18n/icu_util.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

class Environment {
 public:
  Environment() {
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
    CHECK(base::i18n::InitializeICU());
  }
  base::AtExitManager at_exit_manager;
};

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(const base::span<const uint8_t> data) {
  static Environment env;

  bookmarks::BookmarkNodeData bookmark_node_data;
  bookmark_node_data.ReadFromPickle(base::PickleIterator::WithData(data));
  return 0;
}
