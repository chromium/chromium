// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <stddef.h>
#include <sys/socket.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/command_line.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/mac/scoped_ioobject.h"
#include "base/posix/eintr_wrapper.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/common/extensions/image_writer/image_writer_util_mac.h"
#include "chrome/utility/image_writer/disk_unmounter_mac.h"
#include "chrome/utility/image_writer/error_message_strings.h"
#include "chrome/utility/image_writer/image_writer.h"

namespace image_writer {

static const char kAuthOpenPath[] = "/usr/libexec/authopen";

bool ImageWriter::IsValidDevice() {
  base::apple::ScopedCFTypeRef<CFStringRef> cf_bsd_name(
      base::SysUTF8ToCFStringRef(device_path_.value()));
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> matching(
      IOServiceMatching(kIOMediaClass));
  CFDictionaryAddValue(matching.get(), CFSTR(kIOMediaWholeKey), kCFBooleanTrue);
  CFDictionaryAddValue(matching.get(), CFSTR(kIOMediaWritableKey),
                       kCFBooleanTrue);
  CFDictionaryAddValue(matching.get(), CFSTR(kIOBSDNameKey), cf_bsd_name.get());

  // IOServiceGetMatchingService consumes a reference to the matching dictionary
  // passed to it.
  base::mac::ScopedIOObject<io_service_t> disk_obj(
      IOServiceGetMatchingService(kIOMasterPortDefault, matching.release()));
  if (!disk_obj)
    return false;

  return extensions::IsSuitableRemovableStorageDevice(disk_obj.get(), nullptr,
                                                      nullptr, nullptr);
}

void ImageWriter::UnmountVolumes(base::OnceClosure continuation) {
  if (!unmounter_)
    unmounter_ = std::make_unique<DiskUnmounterMac>();

  unmounter_->Unmount(
      device_path_.value(), std::move(continuation),
      base::BindOnce(&ImageWriter::Error, base::Unretained(this),
                     error::kUnmountVolumes));
}

bool ImageWriter::OpenDevice() {
  base::LaunchOptions options;
  options.wait = false;

  // Create a socket pair for communication.
  int sockets[2];
  int result = socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);
  if (result == -1) {
    PLOG(ERROR) << "Unable to allocate socket pair.";
    return false;
  }
  base::ScopedFD parent_socket(sockets[0]);
  base::ScopedFD child_socket(sockets[1]);

  // Map the client socket to the client's STDOUT.
  options.fds_to_remap.push_back(
      std::pair<int, int>(child_socket.get(), STDOUT_FILENO));

  // Find the file path to open.
  base::FilePath real_device_path;
  if (device_path_.IsAbsolute()) {
    // This only occurs for tests where the device path is mocked with a
    // temporary file.
    real_device_path = device_path_;
  } else {
    // Get the raw device file. Writes need to be in multiples of
    // DAMediaBlockSize (usually 512). This is fine since WriteChunk() writes in
    // multiples of kMemoryAlignment.
    real_device_path =
        base::FilePath("/dev").Append("r" + device_path_.BaseName().value());
  }

  // Build the command line.
  std::string rdwr = base::NumberToString(O_RDWR);

  base::CommandLine cmd_line = base::CommandLine(base::FilePath(kAuthOpenPath));
  cmd_line.AppendSwitch("-stdoutpipe");
  // Using AppendSwitchNative will use an equal-symbol which we don't want.
  cmd_line.AppendArg("-o");
  cmd_line.AppendArg(rdwr);
  cmd_line.AppendArgPath(real_device_path);

  // Launch the process.
  base::Process process = base::LaunchProcess(cmd_line, options);
  if (!process.IsValid()) {
    LOG(ERROR) << "Failed to launch authopen process.";
    return false;
  }

  // Receive a file descriptor from authopen which sends a single FD via
  // sendmsg and the SCM_RIGHTS extension.
  int fd = -1;
  const size_t kDataBufferSize = sizeof(struct cmsghdr) + sizeof(int);
  char data_buffer[kDataBufferSize];

  struct iovec io_vec[1];
  io_vec[0].iov_base = data_buffer;
  io_vec[0].iov_len = kDataBufferSize;

  const socklen_t kCmsgSocketSize =
      static_cast<socklen_t>(CMSG_SPACE(sizeof(int)));
  char cmsg_socket[kCmsgSocketSize];

  struct msghdr message = {0};
  message.msg_iov = io_vec;
  message.msg_iovlen = 1;
  message.msg_control = cmsg_socket;
  message.msg_controllen = kCmsgSocketSize;

  ssize_t size = HANDLE_EINTR(recvmsg(parent_socket.get(), &message, 0));
  if (size > 0) {
    struct cmsghdr* cmsg_socket_header = CMSG_FIRSTHDR(&message);

    if (cmsg_socket_header && cmsg_socket_header->cmsg_level == SOL_SOCKET &&
        cmsg_socket_header->cmsg_type == SCM_RIGHTS) {
      fd = *reinterpret_cast<int*>(CMSG_DATA(cmsg_socket_header));
    }
  }

  device_file_ = base::File(fd);

  // Wait for the child.
  int child_exit_status;
  if (!process.WaitForExit(&child_exit_status)) {
    LOG(ERROR) << "Unable to wait for child.";
    return false;
  }

  if (child_exit_status) {
    LOG(ERROR) << "Child process returned failure.";
    return false;
  }

  return device_file_.IsValid();
}

}  // namespace image_writer
