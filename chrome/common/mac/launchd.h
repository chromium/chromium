// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_LAUNCHD_H_
#define CHROME_COMMON_MAC_LAUNCHD_H_

#include <CoreFoundation/CoreFoundation.h>

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

  Launchd(const Launchd&) = delete;
  Launchd& operator=(const Launchd&) = delete;

  virtual ~Launchd();

  virtual bool GetJobInfo(const std::string& label,
                          mac::services::JobInfo* info);

  // Remove a launchd process from launchd.
  virtual bool RemoveJob(const std::string& label);

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

  static NSURL* GetPlistURL(Launchd::Domain domain,
                            Launchd::Type type,
                            CFStringRef name);

 protected:
  Launchd() { }

 private:
  // TODO(dmaclach): remove this once http://crbug.com/76925 is fixed.
  // Scaffolding for doing unittests with our singleton.
  friend struct base::DefaultSingletonTraits<Launchd>;
  static Launchd* g_instance_;
};

#endif  // CHROME_COMMON_MAC_LAUNCHD_H_
