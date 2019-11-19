// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_LAUNCHD_H_
#define CHROME_COMMON_MAC_LAUNCHD_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "chrome/common/mac/service_management.h"

class Launchd {
 public:
  enum Type {
    Agent,  // LaunchAgent
    Daemon  // LaunchDaemon
  };

  // Domains map to NSSearchPathDomainMask so Foundation does not need to be
  // included.
  enum Domain {
    User = 1,  // ~/Library/Launch*
    Local = 2,  // /Library/Launch*
    Network = 4,  // /Network/Library/Launch*
    System = 8  // /System/Library/Launch*
  };

  // TODO(dmaclach): Get rid of this pseudo singleton, and inject it
  // appropriately wherever it is used.
  // http://crbug.com/76925
  static Launchd* GetInstance();

  virtual ~Launchd();

  virtual bool GetJobInfo(const std::string& label,
                          mac::services::JobInfo* info);

  // Remove a launchd process from launchd.
  virtual bool RemoveJob(const std::string& label);

  // Used by a process controlled by launchd to restart itself.
  // |session_type| can be "Aqua", "LoginWindow", "Background", "StandardIO" or
  // "System".
  // RestartLaunchdJob starts up a separate process to tell launchd to
  // send this process a SIGTERM. This call will return, but a SIGTERM will be
  // received shortly.
  virtual bool RestartJob(Domain domain,
                          Type type,
                          CFStringRef name,
                          CFStringRef session_type);

  // Read a launchd plist from disk.
  // |name| should not have an extension.
  virtual CFMutableDictionaryRef CreatePlistFromFile(Domain domain,
                                                     Type type,
                                                     CFStringRef name);
  // Write a launchd plist to disk.
  // |name| should not have an extension.
  virtual bool WritePlistToFile(Domain domain,
                                Type type,
                                CFStringRef name,
                                CFDictionaryRef dict);

  // Delete a launchd plist.
  // |name| should not have an extension.
  virtual bool DeletePlist(Domain domain, Type type, CFStringRef name);

  // TODO(dmaclach): remove this once http://crbug.com/76925 is fixed.
  // Scaffolding for doing unittests with our singleton.
  static void SetInstance(Launchd* instance);
  class ScopedInstance {
   public:
    explicit ScopedInstance(Launchd* instance) {
      Launchd::SetInstance(instance);
    }
    ~ScopedInstance() {
      Launchd::SetInstance(NULL);
    }
  };

 protected:
  Launchd() { }

 private:
  // TODO(dmaclach): remove this once http://crbug.com/76925 is fixed.
  // Scaffolding for doing unittests with our singleton.
  friend struct base::DefaultSingletonTraits<Launchd>;
  static Launchd* g_instance_;

  DISALLOW_COPY_AND_ASSIGN(Launchd);
};

#endif  // CHROME_COMMON_MAC_LAUNCHD_H_
