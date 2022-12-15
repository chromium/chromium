// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_APPEND_ARGUMENTS_FROM_FILE_H_
#define COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_APPEND_ARGUMENTS_FROM_FILE_H_

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace fuchsia_component_support {

// Reads serialized arguments from the optional file at `path` and appends them
// to `command_line`. See SerializeArguments for the converse. Returns true if
// the file was absent or was well-formed and successfully parsed, regardless of
// whether or not any arguments were found within it.
bool AppendArgumentsFromFile(const base::FilePath& path,
                             base::CommandLine& command_line);

}  // namespace fuchsia_component_support

#endif  // COMPONENTS_FUCHSIA_COMPONENT_SUPPORT_APPEND_ARGUMENTS_FROM_FILE_H_
