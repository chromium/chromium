// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_JS_IN_PROCESS_FUZZER_UTILS_H_
#define CHROME_TEST_FUZZING_JS_IN_PROCESS_FUZZER_UTILS_H_

#include <string_view>

namespace js_in_process_fuzzer {

// Get the first line that is not a comment.
std::string_view GetFirstNonCommentLine(std::string_view js);

}  // namespace js_in_process_fuzzer

#endif  // CHROME_TEST_FUZZING_JS_IN_PROCESS_FUZZER_UTILS_H_
