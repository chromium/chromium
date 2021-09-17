// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_lock.h"

#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/strings/sys_string_conversions.h"

#include <servers/bootstrap.h>

class MultiProcessLockMac : public MultiProcessLock {
 public:
  explicit MultiProcessLockMac(const std::string& name) : name_(name) { }

  MultiProcessLockMac(const MultiProcessLockMac&) = delete;
  MultiProcessLockMac& operator=(const MultiProcessLockMac&) = delete;

  ~MultiProcessLockMac() override {
    if (port_ != NULL) {
      Unlock();
    }
  }

  bool TryLock() override {
    if (port_ != NULL) {
      DLOG(ERROR) << "MultiProcessLock is already locked - " << name_;
      return true;
    }

    if (name_.length() >= BOOTSTRAP_MAX_NAME_LEN) {
      LOG(ERROR) << "Socket name too long (" << name_.length()
                 << " >= " << BOOTSTRAP_MAX_NAME_LEN << ") - " << name_;
      return false;
    }

    base::ScopedCFTypeRef<CFStringRef> cf_name =
        base::SysUTF8ToCFStringRef(name_);
    port_.reset(CFMessagePortCreateLocal(NULL, cf_name, NULL, NULL, NULL));
    return port_ != NULL;
  }

  void Unlock() override {
    if (port_ == NULL) {
      DLOG(ERROR) << "Over-unlocked MultiProcessLock - " << name_;
      return;
    }
    port_.reset();
  }

 private:
  std::string name_;
  base::ScopedCFTypeRef<CFMessagePortRef> port_;
};

std::unique_ptr<MultiProcessLock> MultiProcessLock::Create(
    const std::string& name) {
  return std::make_unique<MultiProcessLockMac>(name);
}
