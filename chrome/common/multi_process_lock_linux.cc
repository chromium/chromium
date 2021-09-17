// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_lock.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/posix/eintr_wrapper.h"

class MultiProcessLockLinux : public MultiProcessLock {
 public:
  explicit MultiProcessLockLinux(const std::string& name)
      : name_(name), fd_(-1) { }

  MultiProcessLockLinux(const MultiProcessLockLinux&) = delete;
  MultiProcessLockLinux& operator=(const MultiProcessLockLinux&) = delete;

  ~MultiProcessLockLinux() override {
    if (fd_ != -1) {
      Unlock();
    }
  }

  bool TryLock() override {
    struct sockaddr_un address;

    // +1 for terminator, +1 for 0 in position 0 that makes it an
    // abstract named socket.
    const size_t max_len = sizeof(address.sun_path) - 2;

    if (fd_ != -1) {
      DLOG(ERROR) << "MultiProcessLock is already locked - " << name_;
      return true;
    }

    if (name_.length() > max_len) {
      LOG(ERROR) << "Socket name too long (" << name_.length()
                 << " > " << max_len << ") - " << name_;
      return false;
    }

    memset(&address, 0, sizeof(address));
    int print_length = snprintf(&address.sun_path[1],
                                max_len + 1,
                                "%s", name_.c_str());

    if (print_length < 0 ||
        print_length > static_cast<int>(max_len)) {
      PLOG(ERROR) << "Couldn't create sun_path - " << name_;
      return false;
    }

    // Must set the first character of the path to something non-zero
    // before we call SUN_LEN which depends on strcpy working.
    address.sun_path[0] = '@';
    size_t length = SUN_LEN(&address);

    // Reset the first character of the path back to zero so that
    // bind returns an abstract name socket.
    address.sun_path[0] = 0;
    address.sun_family = AF_LOCAL;

    int socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0);
    if (socket_fd < 0) {
      PLOG(ERROR) << "Couldn't create socket - " << name_;
      return false;
    }

    if (bind(socket_fd,
             reinterpret_cast<sockaddr *>(&address),
             length) == 0) {
      fd_ = socket_fd;
      return true;
    } else {
      DVLOG(1) << "Couldn't bind socket - "
               << &(address.sun_path[1])
               << " Length: " << length;
      if (IGNORE_EINTR(close(socket_fd)) < 0) {
        PLOG(ERROR) << "close";
      }
      return false;
    }
  }

  void Unlock() override {
    if (fd_ == -1) {
      DLOG(ERROR) << "Over-unlocked MultiProcessLock - " << name_;
      return;
    }
    if (IGNORE_EINTR(close(fd_)) < 0) {
      DPLOG(ERROR) << "close";
    }
    fd_ = -1;
  }

 private:
  std::string name_;
  int fd_;
};

std::unique_ptr<MultiProcessLock> MultiProcessLock::Create(
    const std::string& name) {
  return std::make_unique<MultiProcessLockLinux>(name);
}
