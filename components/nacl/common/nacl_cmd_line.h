// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_COMMON_NACL_CMD_LINE_H_
#define COMPONENTS_NACL_COMMON_NACL_CMD_LINE_H_

namespace base {
class CommandLine;
}

namespace nacl {

// Copy all the relevant arguments from the command line of the current
// process to cmd_line that will be used for launching the NaCl loader/broker.
void CopyNaClCommandLineArguments(base::CommandLine* cmd_line);

}  // namespace nacl

#endif  // COMPONENTS_NACL_COMMON_NACL_CMD_LINE_H_
