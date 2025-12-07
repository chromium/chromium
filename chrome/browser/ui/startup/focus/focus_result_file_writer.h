// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_RESULT_FILE_WRITER_H_
#define CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_RESULT_FILE_WRITER_H_

#include <string>

#include "chrome/browser/ui/startup/focus/focus_handler.h"

namespace focus {

// Creates a JSON string with focus result information.
std::string CreateFocusJsonString(const FocusResult& result);

// Writes focus result information to a file in JSON format.
void WriteResultToFile(std::string file_path, const FocusResult& result);

}  // namespace focus

#endif  // CHROME_BROWSER_UI_STARTUP_FOCUS_FOCUS_RESULT_FILE_WRITER_H_
