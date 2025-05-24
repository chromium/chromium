// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/tracing/common/graphics_memory_dump_provider_android.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <string_view>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/trace_event/process_memory_dump.h"

using base::trace_event::MemoryAllocatorDump;

namespace tracing {

// static
const char GraphicsMemoryDumpProvider::kDumpBaseName[] =
    "gpu/android_memtrack/";

// static
GraphicsMemoryDumpProvider* GraphicsMemoryDumpProvider::GetInstance() {
  return base::Singleton<
      GraphicsMemoryDumpProvider,
      base::LeakySingletonTraits<GraphicsMemoryDumpProvider>>::get();
}

bool GraphicsMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  if (args.level_of_detail !=
      base::trace_event::MemoryDumpLevelOfDetail::kDetailed) {
    return true;  // Dump on detailed memory dumps only.
  }

  const char kAbstractSocketName[] = "chrome_tracing_memtrack_helper";
  struct sockaddr_un addr;

  const int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);
  if (sock == -1)
    return false;
  // Ensures that sock is always closed, even in case of early returns.
  base::ScopedFD sock_closer(sock);

  // Set recv() timeout to 250 ms.
  struct timeval tv;
  tv.tv_sec = 0;
  tv.tv_usec = 250000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

  // Connect to the UNIX abstract (i.e. no physical filesystem link) socket.
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(&addr.sun_path[1], kAbstractSocketName, sizeof(addr.sun_path) - 2);

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr))) {
    LOG(WARNING) << "Could not connect to the memtrack_helper daemon. Please "
                    "build memtrack_helper, adb push to the device and run it "
                    "before starting the trace to get graphics memory data.";
    return false;
  }

  // Check that the socket is owned by root (the memtrack_helper) process and
  // not an (untrusted) user process.
  struct ucred cred;
  socklen_t cred_len = sizeof(cred);
  if (getsockopt(sock, SOL_SOCKET, SO_PEERCRED, &cred, &cred_len) < 0 ||
      (static_cast<unsigned>(cred_len) < sizeof(cred)) ||
      cred.uid != 0 /* root */) {
    LOG(WARNING) << "Untrusted (!= root) memtrack_helper daemon detected.";
    return false;
  }

  // Send the trace(PID) request.
  char buf[4096];
  const int buf_pid_len = snprintf(buf, sizeof(buf) - 1, "%d", getpid());
  if (HANDLE_EINTR(send(sock, buf, buf_pid_len + 1, 0)) <= 0)
    return false;

  // The response consists of a few lines, each one with a key value pair. E.g.:
  // graphics_total 10616832
  // graphics_pss 10616832
  // gl_total 17911808
  // gl_pss 17911808
  ssize_t rsize;
  if ((rsize = HANDLE_EINTR(recv(sock, buf, sizeof(buf), 0))) <= 0)
    return false;

  buf[sizeof(buf) - 1] = '\0';
  ParseResponseAndAddToDump(buf, static_cast<size_t>(rsize), pmd);
  return true;
}

void GraphicsMemoryDumpProvider::ParseResponseAndAddToDump(
    const char* response,
    size_t length,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::CStringTokenizer tokenizer(response, response + length, " \n");
  while (true) {
    if (!tokenizer.GetNext())
      break;
    std::string_view row_name = tokenizer.token_piece();
    if (!tokenizer.GetNext())
      break;
    std::string_view value_str = tokenizer.token_piece();
    int64_t value;
    if (!base::StringToInt64(value_str, &value) || value < 0)
      continue;  // Skip invalid or negative values.

    // Turn entries like graphics_total into a row named "graphics" and
    // column named "total".
    std::string column_name = "memtrack_";
    size_t key_split_point = row_name.find_last_of('_');
    if (key_split_point > 0 && key_split_point < row_name.size() - 1) {
      column_name.append(row_name.begin() + key_split_point + 1,
                         row_name.end());
      row_name = row_name.substr(0, key_split_point);
    } else {
      column_name += "unknown";
    }

    // Append a row to the memory dump.
    std::string dump_name = base::StrCat({kDumpBaseName, row_name});
    MemoryAllocatorDump* mad = pmd->GetOrCreateAllocatorDump(dump_name);
    const auto& long_lived_column_name = key_names_.insert(column_name).first;
    mad->AddScalar(long_lived_column_name->c_str(),
                   MemoryAllocatorDump::kUnitsBytes, value);
  }
}

GraphicsMemoryDumpProvider::GraphicsMemoryDumpProvider() = default;

GraphicsMemoryDumpProvider::~GraphicsMemoryDumpProvider() = default;

}  // namespace tracing
