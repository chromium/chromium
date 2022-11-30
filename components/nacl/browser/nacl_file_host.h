// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NACL_BROWSER_NACL_FILE_HOST_H_
#define COMPONENTS_NACL_BROWSER_NACL_FILE_HOST_H_

#include <string>

#include "base/memory/ref_counted.h"

class GURL;

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}

namespace nacl {
class NaClHostMessageFilter;
}

// Opens NaCl Files in the Browser process, on behalf of the NaCl plugin.

namespace nacl_file_host {

// Open a PNaCl file (readonly) on behalf of the NaCl plugin.
// If it is executable, registers the executable for validation caching.
// Otherwise, just opens the file read-only.
void GetReadonlyPnaclFd(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    bool is_executable,
    IPC::Message* reply_msg);

// Return true if the filename requested is valid for opening.
// Sets file_to_open to the base::FilePath which we will attempt to open.
bool PnaclCanOpenFile(const std::string& filename,
                      base::FilePath* file_to_open);

// Opens a NaCl executable file for reading and executing.
void OpenNaClExecutable(
    scoped_refptr<nacl::NaClHostMessageFilter> nacl_host_message_filter,
    int render_frame_id,
    const GURL& file_url,
    IPC::Message* reply_msg);

}  // namespace nacl_file_host

#endif  // COMPONENTS_NACL_BROWSER_NACL_FILE_HOST_H_
