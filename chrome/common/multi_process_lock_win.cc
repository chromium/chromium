// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_lock.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_handle.h"

#include <windows.h>

class MultiProcessLockWin : public MultiProcessLock {
 public:
  explicit MultiProcessLockWin(const std::string& name) : name_(name) { }

  ~MultiProcessLockWin() override {
    if (event_.Get() != NULL) {
      Unlock();
    }
  }

  bool TryLock() override {
    if (event_.Get() != NULL) {
      DLOG(ERROR) << "MultiProcessLock is already locked - " << name_;
      return true;
    }

    if (name_.length() >= MAX_PATH) {
      LOG(ERROR) << "Socket name too long (" << name_.length()
                 << " >= " << MAX_PATH << ") - " << name_;
      return false;
    }

    std::wstring wname = base::UTF8ToWide(name_);
    event_.Set(CreateEvent(NULL, FALSE, FALSE, wname.c_str()));
    if (event_.Get() && GetLastError() != ERROR_ALREADY_EXISTS) {
      return true;
    } else {
      event_.Set(NULL);
      return false;
    }
  }

  void Unlock() override {
    if (event_.Get() == NULL) {
      DLOG(ERROR) << "Over-unlocked MultiProcessLock - " << name_;
      return;
    }
    event_.Set(NULL);
  }

 private:
  std::string name_;
  base::win::ScopedHandle event_;
  DISALLOW_COPY_AND_ASSIGN(MultiProcessLockWin);
};

std::unique_ptr<MultiProcessLock> MultiProcessLock::Create(
    const std::string& name) {
  return std::make_unique<MultiProcessLockWin>(name);
}
